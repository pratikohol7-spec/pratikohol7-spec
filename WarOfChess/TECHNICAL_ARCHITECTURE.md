# WAR OF CHESS: Technical Architecture (initial design)

This is the planned design. Nothing below is implemented yet.

## Core principle

```
 Input (touch) ──► GameController ──► ChessEngine (pure C#)  ── decides legality & results
                         │                    │
                         │◄── MoveResult ─────┘
                         ▼
                  PresentationQueue ──► Board view / Warriors / CombatDirector / Camera / Audio
                         │
                         └── signals "presentation finished" ──► next turn unlocked
```

1. **ChessEngine** (`Scripts/Chess/`, plain C#, no `UnityEngine` references) holds the board state, move generation, legality, check, checkmate, stalemate, castling, en passant, promotion, draws (50-move, threefold repetition, insufficient material), undo, FEN import/export, and move history. Unity Edit Mode unit tests cover it.
2. **GameController** (`Scripts/Core/`) owns the turn state machine: `AwaitingInput → Presenting → (GameOver | AwaitingInput)`. Input is ignored while `Presenting`, so a player cannot make a second move during an animation.
3. **Presentation** never decides anything. It receives the engine's `MoveResult` (from, to, captured piece, special-move flags) and plays it.
   - `Scripts/Characters/`: warrior view per piece type, mapped by a `PieceVisualSet` ScriptableObject.
   - `Scripts/Combat/`: `CombatDirector` chooses a sequence from a table keyed by (attacker type, defender type). Settings: Full / Short / Off.
   - `Scripts/Camera/`: isometric board camera, zoom/rotate limits, cinematic transitions (Cinemachine, a free Unity package).
4. **AI** (`Scripts/AI/`) runs a minimax / alpha-beta search over the same ChessEngine, with material + piece-square evaluation. Difficulty controls search depth, time budget and a small deliberate mistake rate on Easy. It runs off the main thread or in time slices so the UI never freezes.
5. **UI** (`Scripts/UI/`) uses Unity UI with a Canvas Scaler and safe-area handling for iPhone.
6. **Platform** (`Scripts/Platform/`) holds the small amount of Android/iOS-specific code (frame rate, haptics, safe area).
7. **Audio** (`Scripts/Audio/`) has separate Music and SFX mixer groups, each with its own mute and volume.

## Game modes

A `GameModeConfig` (ScriptableObject) describes the players (Human/AI and difficulty), the starting position (FEN), and the rules for advancing (campaign/survival). Online play would later swap the local engine authority for a server, but use the same `MoveResult` contract.

## Packages (kept minimal)

URP, Input System, Cinemachine, Unity Test Framework, TextMeshPro/uGUI. Anything else needs a reason and your approval.

## Performance budget (targets, to verify on device)

60 fps target (30 fps on Low), under 150k on-screen triangles, under 150 draw calls on the board view, GPU instancing / SRP Batcher, baked lighting with one real-time shadow-casting light (shadows off on Low), ASTC texture compression, object pooling for VFX.
