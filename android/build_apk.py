#!/usr/bin/env python3
"""Build a signed Android APK of Ember & the Iron Tide without the Android SDK.

Pipeline:
  1. Bundle game/index.html into one offline page (Three.js + fonts inlined) with esbuild.
  2. Compile MainActivity.java against the android-4.1.1.4 stub jar and convert it to DEX with dx.
  3. Encode AndroidManifest.xml (binary XML) and resources.arsc (one icon drawable) directly.
  4. Zip with 4-byte alignment, then sign (v1 + v2) and verify with apksig.

Needs: python3 + Pillow, node/npm, a JDK (javac/java/keytool) and network access to
Maven Central and the npm registry (tools are cached in android/.cache).

Usage:  python3 android/build_apk.py            -> android/dist/EmberIronTide.apk
"""
import base64, io, os, re, shutil, struct, subprocess, sys, time, urllib.request, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AND = os.path.join(ROOT, 'android')
CACHE = os.path.join(AND, '.cache')
BUILD = os.path.join(CACHE, 'build')
DIST = os.path.join(AND, 'dist')
PACKAGE = 'com.pratikohol.irontide'
APP_NAME = 'Iron Tide'
VERSION_CODE, VERSION_NAME = 2, '1.1'
MIN_SDK, TARGET_SDK = 24, 34
KEYSTORE = os.environ.get('IRONTIDE_KEYSTORE', os.path.join(AND, 'release.p12'))
KEYPASS = os.environ.get('IRONTIDE_KEYPASS', 'irontide')
ALIAS = 'irontide'

MAVEN = 'https://repo1.maven.org/maven2/'
JARS = {
    'android.jar': 'com/google/android/android/4.1.1.4/android-4.1.1.4.jar',
    'dx.jar': 'com/jakewharton/android/repackaged/dalvik-dx/16.0.1/dalvik-dx-16.0.1.jar',
    'apksig.jar': 'com/android/tools/build/apksig/2.3.0/apksig-2.3.0.jar',
}
NPM = ['three@0.160.0', 'esbuild@0.21.5', '@fontsource/limelight@5', '@fontsource/barlow-condensed@5']


def run(cmd, **kw):
    print('  $', ' '.join(cmd) if isinstance(cmd, list) else cmd)
    subprocess.run(cmd, check=True, **kw)


def fetch_tools():
    os.makedirs(CACHE, exist_ok=True)
    for name, path in JARS.items():
        dest = os.path.join(CACHE, name)
        if os.path.exists(dest) and zipfile.is_zipfile(dest):
            continue
        for wait in (0, 10, 30, 60, 120):  # Maven Central rate-limits bursts
            time.sleep(wait)
            try:
                urllib.request.urlretrieve(MAVEN + path, dest)
                if zipfile.is_zipfile(dest):
                    break
            except Exception as e:
                print('  retrying', name, e)
        else:
            sys.exit('could not download ' + name)
    if not os.path.exists(os.path.join(CACHE, 'node_modules', 'esbuild')):
        if not os.path.exists(os.path.join(CACHE, 'package.json')):
            open(os.path.join(CACHE, 'package.json'), 'w').write('{"private":true}')
        run(['npm', 'install', '--no-audit', '--no-fund', '--silent'] + NPM, cwd=CACHE)


# ---------------------------------------------------------------- web bundle
def bundle_web(out_dir):
    src = open(os.path.join(ROOT, 'game', 'index.html'), encoding='utf-8').read()
    m = re.search(r'<script type="module">(.*?)</script>', src, re.S)
    entry = os.path.join(BUILD, 'game.js')
    open(entry, 'w', encoding='utf-8').write(m.group(1))
    out_js = os.path.join(BUILD, 'game.bundle.js')
    run([os.path.join(CACHE, 'node_modules', '.bin', 'esbuild'), entry, '--bundle', '--format=iife', '--minify',
         '--target=es2020', '--outfile=' + out_js, '--log-level=warning'],
        env={**os.environ, 'NODE_PATH': os.path.join(CACHE, 'node_modules')}, cwd=CACHE)
    js = open(out_js, encoding='utf-8').read().replace('</script', '<\\/script')
    # embed fonts so the game looks right offline
    fdir = os.path.join(CACHE, 'node_modules', '@fontsource')
    faces = [('Limelight', 400, 'limelight/files/limelight-latin-400-normal.woff2')] + \
            [('Barlow Condensed', w, f'barlow-condensed/files/barlow-condensed-latin-{w}-normal.woff2') for w in (400, 600, 800)]
    css = ''.join("@font-face{font-family:'%s';font-weight:%d;font-style:normal;font-display:block;src:url(data:font/woff2;base64,%s) format('woff2')}"
                  % (fam, w, base64.b64encode(open(os.path.join(fdir, p), 'rb').read()).decode()) for fam, w, p in faces)
    html = src[:m.start()] + '<script>' + js + '</script>' + src[m.end():]
    html = re.sub(r'<script type="importmap">.*?</script>\s*', '', html, flags=re.S)
    html = re.sub(r'<link rel="(preconnect|stylesheet)" href="https://fonts[^>]*>\s*', '', html)
    html = html.replace('<style>', '<style>' + css, 1)
    os.makedirs(out_dir, exist_ok=True)
    open(os.path.join(out_dir, 'index.html'), 'w', encoding='utf-8').write(html)
    print('  web bundle: %.0f KB' % (len(html.encode()) / 1024))


# ---------------------------------------------------------------- binary XML
ANDROID_NS = 'http://schemas.android.com/apk/res/android'
ATTR_IDS = {  # android.R.attr values (verified against android-4.1.1.4.jar)
    'theme': 0x01010000, 'label': 0x01010001, 'icon': 0x01010002, 'name': 0x01010003,
    'exported': 0x01010010, 'launchMode': 0x0101001d, 'screenOrientation': 0x0101001e,
    'configChanges': 0x0101001f, 'minSdkVersion': 0x0101020c, 'versionCode': 0x0101021b,
    'versionName': 0x0101021c, 'targetSdkVersion': 0x01010270, 'allowBackup': 0x01010280,
    'hardwareAccelerated': 0x010102d3,
}
T_REF, T_STRING, T_INT, T_BOOL = 0x01, 0x03, 0x10, 0x12


def string_pool(strings, utf8=False):
    if utf8:
        data, offs = b'', []
        for s in strings:
            offs.append(len(data))
            b = s.encode('utf-8')
            assert len(s) < 128 and len(b) < 128
            data += bytes([len(s), len(b)]) + b + b'\0'
        flags = 0x100
    else:
        data, offs = b'', []
        for s in strings:
            offs.append(len(data))
            data += struct.pack('<H', len(s)) + s.encode('utf-16-le') + b'\0\0'
        flags = 0
    data += b'\0' * (-len(data) % 4)
    start = 28 + 4 * len(strings)
    body = b''.join(struct.pack('<I', o) for o in offs) + data
    return struct.pack('<HHIIIIII', 0x0001, 28, 28 + len(body), len(strings), 0, flags, start, 0) + body


def axml(tree):
    """tree: (tag, [(ns, name, type, value)], [children]) -> binary AndroidManifest.xml"""
    res_names, other = [], []

    def collect(node):
        tag, attrs, kids = node
        for ns, name, typ, val in attrs:
            if ns and name not in res_names:
                res_names.append(name)
        for k in kids:
            collect(k)
    collect(tree)
    strings = list(res_names)

    def idx(s):
        if s not in strings:
            strings.append(s)
        return strings.index(s)
    idx('android'); idx(ANDROID_NS)
    chunks = []
    line = [1]

    def node_chunks(node):
        tag, attrs, kids = node
        enc = []
        for ns, name, typ, val in attrs:
            raw = 0xFFFFFFFF
            if typ == T_STRING:
                data = raw = idx(val)
            elif typ == T_BOOL:
                data = 0xFFFFFFFF if val else 0
            else:
                data = val
            enc.append((ATTR_IDS.get(name, 0xFFFFFFFF) if ns else 0xFFFFFFFF, idx(ANDROID_NS) if ns else 0xFFFFFFFF,
                        idx(name), raw, typ, data))
        enc.sort(key=lambda a: a[0])  # Android expects attributes ordered by resource id
        body = struct.pack('<IIHHHHHH', 0xFFFFFFFF, idx(tag), 20, 20, len(enc), 0, 0, 0)
        for _, ns, name, raw, typ, data in enc:
            body += struct.pack('<IIIHBBI', ns, name, raw, 8, 0, typ, data)
        chunks.append(struct.pack('<HHIII', 0x0102, 16, 16 + len(body), line[0], 0xFFFFFFFF) + body)
        line[0] += 1
        for k in kids:
            node_chunks(k)
        chunks.append(struct.pack('<HHIIIII', 0x0103, 16, 24, line[0], 0xFFFFFFFF, 0xFFFFFFFF, idx(tag)))
        line[0] += 1

    node_chunks(tree)
    ns_start = struct.pack('<HHIIIII', 0x0100, 16, 24, 1, 0xFFFFFFFF, idx('android'), idx(ANDROID_NS))
    ns_end = struct.pack('<HHIIIII', 0x0101, 16, 24, line[0], 0xFFFFFFFF, idx('android'), idx(ANDROID_NS))
    pool = string_pool(strings)
    resmap = struct.pack('<HHI', 0x0180, 8, 8 + 4 * len(res_names)) + b''.join(struct.pack('<I', ATTR_IDS[n]) for n in res_names)
    body = pool + resmap + ns_start + b''.join(chunks) + ns_end
    return struct.pack('<HHI', 0x0003, 8, 8 + len(body)) + body


def manifest():
    A = True
    return ('manifest', [
        (None, 'package', T_STRING, PACKAGE),
        (A, 'versionCode', T_INT, VERSION_CODE),
        (A, 'versionName', T_STRING, VERSION_NAME),
    ], [
        ('uses-sdk', [(A, 'minSdkVersion', T_INT, MIN_SDK), (A, 'targetSdkVersion', T_INT, TARGET_SDK)], []),
        ('uses-feature', [(A, 'name', T_STRING, 'android.hardware.touchscreen')], []),
        ('application', [
            (A, 'label', T_STRING, APP_NAME),
            (A, 'icon', T_REF, 0x7F010000),
            (A, 'hardwareAccelerated', T_BOOL, True),
            (A, 'allowBackup', T_BOOL, True),
            (A, 'theme', T_REF, 0x01030007),  # @android:style/Theme.NoTitleBar.Fullscreen
        ], [
            ('activity', [
                (A, 'name', T_STRING, PACKAGE + '.MainActivity'),
                (A, 'exported', T_BOOL, True),
                (A, 'launchMode', T_INT, 2),            # singleTask
                (A, 'screenOrientation', T_INT, 6),     # sensorLandscape
                (A, 'configChanges', T_INT, 0x4B0 | 0x80 | 0x20 | 0x10 | 0x400 | 0x100 | 0x200),
                (A, 'label', T_STRING, APP_NAME),
            ], [
                ('intent-filter', [], [
                    ('action', [(A, 'name', T_STRING, 'android.intent.action.MAIN')], []),
                    ('category', [(A, 'name', T_STRING, 'android.intent.category.LAUNCHER')], []),
                ]),
            ]),
        ]),
    ])


def arsc_icon():
    """resources.arsc with a single entry: drawable/icon (0x7f010000) -> res/drawable/icon.png"""
    gpool = string_pool(['res/drawable/icon.png'], utf8=True)
    tpool = string_pool(['drawable'])
    kpool = string_pool(['icon'])
    spec = struct.pack('<HHIBBHI', 0x0202, 16, 20, 1, 0, 0, 1) + struct.pack('<I', 0)
    config = struct.pack('<I', 64) + b'\0' * 60
    entry = struct.pack('<HHI', 8, 0, 0) + struct.pack('<HBBI', 8, 0, T_STRING, 0)
    hdr = 20 + len(config)
    typ = struct.pack('<HHIBBHII', 0x0201, hdr, hdr + 4 + len(entry), 1, 0, 0, 1, hdr + 4) + config + struct.pack('<I', 0) + entry
    name = PACKAGE.encode('utf-16-le').ljust(256, b'\0')
    pkg_hdr = 288
    type_off = pkg_hdr
    key_off = pkg_hdr + len(tpool)
    pkg_body = tpool + kpool + spec + typ
    pkg = struct.pack('<HHII', 0x0200, pkg_hdr, pkg_hdr + len(pkg_body), 0x7F) + name + \
        struct.pack('<IIIII', type_off, 1, key_off, 1, 0) + pkg_body
    body = gpool + pkg
    return struct.pack('<HHII', 0x0002, 12, 12 + len(body), 1) + body


# ---------------------------------------------------------------- icon
def make_icon(size=192):
    from PIL import Image, ImageDraw, ImageFilter
    import math
    S = size * 4
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, S - 1, S - 1], radius=S // 5, fill=(24, 16, 14, 255))
    for r in range(S // 2, 0, -4):  # warm sunset glow
        t = r / (S / 2)
        d.ellipse([S / 2 - r, S * .62 - r, S / 2 + r, S * .62 + r], fill=(int(60 + 140 * (1 - t)), int(26 + 50 * (1 - t)), 18, 255))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=S // 5, fill=255)
    d = ImageDraw.Draw(im)
    cx, cy, R = S / 2, S / 2, S * .34
    pts = []
    for i in range(48):  # brass cog
        a = i / 48 * 2 * math.pi
        rr = R * (1.12 if (i // 2) % 2 == 0 else .98)
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=(201, 150, 70, 255))
    d.ellipse([cx - R * .9, cy - R * .9, cx + R * .9, cy + R * .9], fill=(150, 100, 40, 255))
    d.ellipse([cx - R * .78, cy - R * .78, cx + R * .78, cy + R * .78], fill=(226, 176, 90, 255))
    d.ellipse([cx - R * .56, cy - R * .56, cx + R * .56, cy + R * .56], fill=(60, 30, 20, 255))
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([cx - R * .5, cy - R * .5, cx + R * .5, cy + R * .5], fill=(255, 120, 30, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(S * .03))
    im.alpha_composite(glow)
    d = ImageDraw.Draw(im)
    d.ellipse([cx - R * .38, cy - R * .38, cx + R * .38, cy + R * .38], fill=(255, 196, 90, 255))
    d.rounded_rectangle([cx - R * .07, cy - R * .34, cx + R * .07, cy + R * .34], radius=R * .07, fill=(40, 10, 4, 255))
    for i in range(8):  # rivets
        a = i / 8 * 2 * math.pi + .39
        x, y = cx + math.cos(a) * R * .67, cy + math.sin(a) * R * .67
        d.ellipse([x - S * .018, y - S * .018, x + S * .018, y + S * .018], fill=(110, 70, 30, 255))
    im.putalpha(Image.composite(im.getchannel('A'), Image.new('L', (S, S), 0), mask))
    buf = io.BytesIO()
    im.resize((size, size), Image.LANCZOS).save(buf, 'PNG', optimize=True)
    return buf.getvalue()


# ---------------------------------------------------------------- dex
def build_dex():
    cls = os.path.join(BUILD, 'classes')
    shutil.rmtree(cls, ignore_errors=True)
    os.makedirs(cls)
    srcs = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(AND, 'src')) for f in fs if f.endswith('.java')]
    run(['javac', '-nowarn', '-Xlint:-options', '--release', '8', '-cp',
         os.path.join(CACHE, 'android.jar'), '-d', cls] + srcs)
    dex_dir = os.path.join(BUILD, 'dex')
    os.makedirs(dex_dir, exist_ok=True)
    run(['java', '-cp', os.path.join(CACHE, 'dx.jar'), 'com.android.dx.command.Main', '--dex', '--min-sdk-version=24',
         '--output=' + os.path.join(dex_dir, 'classes.dex'), cls])
    return open(os.path.join(dex_dir, 'classes.dex'), 'rb').read()


# ---------------------------------------------------------------- zip + sign
def write_aligned_zip(path, entries):
    """entries: list of (name, bytes, store). Stored entries get 4-byte data alignment (like zipalign)."""
    with zipfile.ZipFile(path, 'w') as z:
        for name, data, store in entries:
            zi = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_STORED if store else zipfile.ZIP_DEFLATED
            zi.create_system = 0
            if store:
                off = z.fp.tell() + 30 + len(name.encode())
                pad = (-(off + 4)) % 4
                zi.extra = struct.pack('<HH', 0xD935, pad) + b'\0' * pad
            z.writestr(zi, data)


def ensure_keystore():
    if os.path.exists(KEYSTORE):
        return
    print('  creating signing key', KEYSTORE)
    run(['keytool', '-genkeypair', '-keystore', KEYSTORE, '-storetype', 'PKCS12', '-storepass', KEYPASS,
         '-keypass', KEYPASS, '-alias', ALIAS, '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000',
         '-dname', 'CN=Iron Tide, O=Pratik Ohol, C=IN'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def sign(unsigned, out):
    tools = os.path.join(BUILD, 'signtool')
    os.makedirs(tools, exist_ok=True)
    jar = os.path.join(CACHE, 'apksig.jar')
    run(['javac', '-nowarn', '-cp', jar, '-d', tools, os.path.join(AND, 'tools', 'Sign.java')])
    opens = [f for pkg in ('sun.security.x509', 'sun.security.pkcs', 'sun.security.util') for f in ('--add-exports', 'java.base/%s=ALL-UNNAMED' % pkg)]
    run(['java'] + opens + ['-cp', jar + os.pathsep + tools, 'Sign', unsigned, out, KEYSTORE, KEYPASS, ALIAS])


def main():
    print('[1/5] tools'); fetch_tools()
    os.makedirs(BUILD, exist_ok=True)
    www = os.path.join(BUILD, 'www')
    print('[2/5] web bundle'); bundle_web(www)
    print('[3/5] dex'); dex = build_dex()
    print('[4/5] package')
    entries = [('AndroidManifest.xml', axml(manifest()), False),
               ('classes.dex', dex, False),
               ('resources.arsc', arsc_icon(), True),
               ('res/drawable/icon.png', make_icon(), True)]
    for dp, _, fs in os.walk(www):
        for f in fs:
            p = os.path.join(dp, f)
            entries.append(('assets/www/' + os.path.relpath(p, www).replace(os.sep, '/'), open(p, 'rb').read(), False))
    unsigned = os.path.join(BUILD, 'unsigned.apk')
    write_aligned_zip(unsigned, entries)
    print('[5/5] sign'); ensure_keystore()
    os.makedirs(DIST, exist_ok=True)
    out = os.path.join(DIST, 'EmberIronTide.apk')
    sign(unsigned, out)
    print('built %s (%.1f MB)' % (out, os.path.getsize(out) / 1e6))


if __name__ == '__main__':
    main()
