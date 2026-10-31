#!/usr/bin/env python3
"""Build a signed Android APK for Nexus City without the Android SDK.

Pipeline: javac (against tiny framework stubs) -> dx (dalvik-dx from Maven Central)
-> hand-encoded binary AndroidManifest.xml + resources.arsc -> 4-byte aligned zip
-> APK Signature Scheme v2 (RSA-2048 / SHA-256).

Usage:  python3 build.py [--key signing-key.pem] [--out NexusCity.apk]
Needs:  JDK 11+, python `cryptography`, and tools/dx.jar
        (https://repo1.maven.org/maven2/com/jakewharton/android/repackaged/dalvik-dx/9.0.0_r3/dalvik-dx-9.0.0_r3.jar)
"""
import argparse, datetime, hashlib, io, os, shutil, struct, subprocess, sys, tempfile, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(HERE, '..', 'nexus-city')
PACKAGE = 'com.nexuscity.game'
VERSION_CODE, VERSION_NAME = 1, '1.0'
MIN_SDK, TARGET_SDK = 24, 34
LABEL = 'Nexus City'

# ---------------------------------------------------------------- binary XML / resources
def u16(v): return struct.pack('<H', v)
def u32(v): return struct.pack('<I', v & 0xFFFFFFFF)
def pad4(b): return b + b'\0' * (-len(b) % 4)

def string_pool(strings):
    offs, data = [], b''
    for s in strings:
        offs.append(len(data))
        enc = s.encode('utf-16-le')
        data += u16(len(s)) + enc + b'\0\0'
    data = pad4(data)
    header_size = 28
    strings_start = header_size + 4 * len(strings)
    body = b''.join(u32(o) for o in offs) + data
    return u16(0x0001) + u16(header_size) + u32(header_size + len(body)) + u32(len(strings)) + u32(0) + u32(0) + u32(strings_start) + u32(0) + body

ANDROID_NS = 'http://schemas.android.com/apk/res/android'
ATTR_IDS = {
    'theme': 0x01010000, 'label': 0x01010001, 'icon': 0x01010002, 'name': 0x01010003, 'exported': 0x01010010,
    'screenOrientation': 0x0101001e, 'configChanges': 0x0101001f, 'minSdkVersion': 0x0101020c,
    'versionCode': 0x0101021b, 'versionName': 0x0101021c, 'targetSdkVersion': 0x01010270, 'hardwareAccelerated': 0x010102d3,
}
T_REF, T_STRING, T_INT, T_HEX, T_BOOL = 0x01, 0x03, 0x10, 0x11, 0x12
ICON_RES_ID = 0x7f010000
THEME_FULLSCREEN = 0x01030007  # @android:style/Theme.NoTitleBar.Fullscreen

def manifest_tree():
    A = lambda n, t, v: (True, n, t, v)      # android: attribute
    P = lambda n, v: (False, n, T_STRING, v)  # plain attribute
    return ('manifest', [P('package', PACKAGE), A('versionCode', T_INT, VERSION_CODE), A('versionName', T_STRING, VERSION_NAME)], [
        ('uses-sdk', [A('minSdkVersion', T_INT, MIN_SDK), A('targetSdkVersion', T_INT, TARGET_SDK)], []),
        ('application', [A('theme', T_REF, THEME_FULLSCREEN), A('label', T_STRING, LABEL), A('icon', T_REF, ICON_RES_ID),
                         A('hardwareAccelerated', T_BOOL, True)], [
            ('activity', [A('name', T_STRING, PACKAGE + '.MainActivity'), A('exported', T_BOOL, True),
                          A('screenOrientation', T_INT, 6),           # sensorLandscape
                          A('configChanges', T_HEX, 0xFB0)], [        # orientation|screenSize|keyboard|... handled in-app
                ('intent-filter', [], [
                    ('action', [A('name', T_STRING, 'android.intent.action.MAIN')], []),
                    ('category', [A('name', T_STRING, 'android.intent.category.LAUNCHER')], []),
                ]),
            ]),
        ]),
    ])

def encode_axml(tree):
    # attribute names with resource ids must come first, in the same order as the resource map
    attr_names = []
    def walk_attrs(node):
        for ns, n, _, _ in node[1]:
            if ns and n not in attr_names: attr_names.append(n)
        for c in node[2]: walk_attrs(c)
    walk_attrs(tree)
    attr_names.sort(key=lambda n: ATTR_IDS[n])
    strings = list(attr_names)
    def idx(s):
        if s not in strings: strings.append(s)
        return strings.index(s)
    for s in ('android', ANDROID_NS): idx(s)
    body = []
    def walk(node):
        tag, attrs, kids = node
        attrs = sorted(attrs, key=lambda a: ATTR_IDS.get(a[1], 0) if a[0] else 0)
        enc = b''
        for ns, n, t, v in attrs:
            raw = 0xFFFFFFFF
            if t == T_STRING: data = idx(v); raw = data
            elif t == T_BOOL: data = 0xFFFFFFFF if v else 0
            else: data = v
            enc += u32(idx(ANDROID_NS) if ns else 0xFFFFFFFF) + u32(idx(n)) + u32(raw) + u16(8) + b'\0' + bytes([t]) + u32(data)
        ext = u32(0xFFFFFFFF) + u32(idx(tag)) + u16(20) + u16(20) + u16(len(attrs)) + u16(0) + u16(0) + u16(0)
        chunk = ext + enc
        body.append(u16(0x0102) + u16(16) + u32(16 + len(chunk)) + u32(1) + u32(0xFFFFFFFF) + chunk)
        for k in kids: walk(k)
        body.append(u16(0x0103) + u16(16) + u32(24) + u32(1) + u32(0xFFFFFFFF) + u32(0xFFFFFFFF) + u32(idx(tag)))
    walk(tree)
    ns_start = u16(0x0100) + u16(16) + u32(24) + u32(1) + u32(0xFFFFFFFF) + u32(idx('android')) + u32(idx(ANDROID_NS))
    ns_end = u16(0x0101) + u16(16) + u32(24) + u32(1) + u32(0xFFFFFFFF) + u32(idx('android')) + u32(idx(ANDROID_NS))
    pool = string_pool(strings)
    resmap_body = b''.join(u32(ATTR_IDS[n]) for n in attr_names)
    resmap = u16(0x0180) + u16(8) + u32(8 + len(resmap_body)) + resmap_body
    content = pool + resmap + ns_start + b''.join(body) + ns_end
    return u16(0x0003) + u16(8) + u32(8 + len(content)) + content

def encode_arsc(icon_path):
    """resources.arsc with a single entry: mipmap/ic_launcher (0x7f010000) -> icon_path."""
    global_pool = string_pool([icon_path])
    type_pool, key_pool = string_pool(['mipmap']), string_pool(['ic_launcher'])
    spec = u16(0x0202) + u16(16) + u32(16 + 4) + bytes([1, 0]) + u16(0) + u32(1) + u32(0)
    config = u32(64) + b'\0' * 60
    entry = u16(8) + u16(0) + u32(0) + u16(8) + b'\0' + bytes([T_STRING]) + u32(0)
    type_header_size = 20 + len(config)
    entries_start = type_header_size + 4
    typ = u16(0x0201) + u16(type_header_size) + u32(entries_start + len(entry)) + bytes([1, 0]) + u16(0) + u32(1) + u32(entries_start) + config + u32(0) + entry
    name = PACKAGE.encode('utf-16-le').ljust(256, b'\0')
    pkg_header_size = 288
    type_strings_off = pkg_header_size
    key_strings_off = type_strings_off + len(type_pool)
    pkg_body = type_pool + key_pool + spec + typ
    pkg = u16(0x0200) + u16(pkg_header_size) + u32(pkg_header_size + len(pkg_body)) + u32(0x7f) + name + \
        u32(type_strings_off) + u32(1) + u32(key_strings_off) + u32(1) + u32(0) + pkg_body
    content = global_pool + pkg
    return u16(0x0002) + u16(12) + u32(12 + len(content)) + u32(1) + content

# ---------------------------------------------------------------- APK signature scheme v2
def lp(b): return u32(len(b)) + b

def chunk_digest(sections):
    digests = []
    for sec in sections:
        for i in range(0, len(sec), 1 << 20):
            c = sec[i:i + (1 << 20)]
            digests.append(hashlib.sha256(b'\xa5' + u32(len(c)) + c).digest())
    return hashlib.sha256(b'\x5a' + u32(len(digests)) + b''.join(digests)).digest()

def sign_v2(apk_bytes, key, cert_der):
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding
    eocd = apk_bytes.rfind(b'PK\x05\x06')
    cd_off = struct.unpack('<I', apk_bytes[eocd + 16:eocd + 20])[0]
    entries, cd, eocd_b = apk_bytes[:cd_off], apk_bytes[cd_off:eocd], apk_bytes[eocd:]
    ALGO = 0x0103  # RSASSA-PKCS1-v1_5 with SHA2-256
    digest = chunk_digest([entries, cd, eocd_b])
    signed_data = lp(lp(u32(ALGO) + lp(digest))) + lp(lp(cert_der)) + lp(b'')
    sig = key.sign(signed_data, padding.PKCS1v15(), hashes.SHA256())
    pub = key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    signer = lp(signed_data) + lp(lp(u32(ALGO) + lp(sig))) + lp(pub)
    value = lp(lp(signer))
    pair = struct.pack('<Q', 4 + len(value)) + u32(0x7109871a) + value
    size = len(pair) + 8 + 16
    block = struct.pack('<Q', size) + pair + struct.pack('<Q', size) + b'APK Sig Block 42'
    new_eocd = eocd_b[:16] + u32(cd_off + len(block)) + eocd_b[20:]
    return entries + block + cd + new_eocd

def load_or_make_key(path):
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID
    if os.path.exists(path):
        key = serialization.load_pem_private_key(open(path, 'rb').read(), None)
    else:
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        open(path, 'wb').write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        print('generated new signing key:', path)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Nexus City'), x509.NameAttribute(NameOID.ORGANIZATION_NAME, 'Nexus City')])
    now = datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
            .serial_number(int.from_bytes(hashlib.sha256(key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)).digest()[:8], 'big'))
            .not_valid_before(now).not_valid_after(now + datetime.timedelta(days=365 * 30)).sign(key, hashes.SHA256()))
    return key, cert.public_bytes(serialization.Encoding.DER)

# ---------------------------------------------------------------- build
def aligned_zip(files):
    """files: list of (name, bytes, compress). Stored entries are 4-byte aligned (zipalign)."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w') as z:
        for name, data, compress in files:
            zi = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            zi.create_system = 0
            zi.compress_type = zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED
            if not compress:
                off = buf.tell() + 30 + len(name.encode())
                zi.extra = b'\0' * (-off % 4)
            z.writestr(zi, data, compresslevel=9 if compress else None)
    return buf.getvalue()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--key', default=os.path.join(HERE, 'signing-key.pem'))
    ap.add_argument('--out', default=os.path.join(HERE, 'NexusCity.apk'))
    ap.add_argument('--dx', default=os.path.join(HERE, 'tools', 'dx.jar'))
    args = ap.parse_args()
    if not os.path.exists(args.dx): sys.exit('missing dx.jar — see the docstring for the download URL')
    tmp = tempfile.mkdtemp()
    try:
        stub_out, cls_out = os.path.join(tmp, 'stubs'), os.path.join(tmp, 'classes')
        stubs = [os.path.join(r, f) for r, _, fs in os.walk(os.path.join(HERE, 'stubs')) for f in fs if f.endswith('.java')]
        srcs = [os.path.join(r, f) for r, _, fs in os.walk(os.path.join(HERE, 'src')) for f in fs if f.endswith('.java')]
        subprocess.check_call(['javac', '-nowarn', '--release', '8', '-d', stub_out] + stubs)
        subprocess.check_call(['javac', '--release', '8', '-cp', stub_out, '-d', cls_out] + srcs)
        dex = os.path.join(tmp, 'classes.dex')
        subprocess.check_call(['java', '-cp', args.dx, 'com.android.dx.command.Main', '--dex', f'--min-sdk-version={MIN_SDK}', f'--output={dex}', cls_out])
        files = [
            ('AndroidManifest.xml', encode_axml(manifest_tree()), True),
            ('classes.dex', open(dex, 'rb').read(), True),
            ('resources.arsc', encode_arsc('res/mipmap/ic_launcher.png'), False),
            ('res/mipmap/ic_launcher.png', open(os.path.join(HERE, 'res', 'ic_launcher.png'), 'rb').read(), False),
        ]
        for root, dirs, fs in os.walk(GAME):
            dirs.sort()
            for f in sorted(fs):
                p = os.path.join(root, f); rel = os.path.relpath(p, GAME).replace(os.sep, '/')
                if rel == 'README.md': continue
                data = open(p, 'rb').read()
                if rel == 'index.html':
                    data = data.replace(b'Best in landscape \xc2\xb7 Add to Home Screen for full-screen play', b'Nexus City \xc2\xb7 Android build v' + VERSION_NAME.encode())
                files.append(('assets/www/' + rel, data, True))
        key, cert = load_or_make_key(args.key)
        apk = sign_v2(aligned_zip(files), key, cert)
        open(args.out, 'wb').write(apk)
        print(f'built {args.out} ({len(apk) / 1024:.0f} KB)')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

if __name__ == '__main__':
    main()
