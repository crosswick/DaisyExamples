# WavPlayer (SDRAM-buffered)

This variant of the seed/WavPlayer example eliminates clicks/ticks when streaming WAV from SD by buffering audio in external SDRAM and reading the SD card in half-buffer bursts.

Why
- Avoid SD/CPU contention that can cause underruns and ticks (see electro-smith/libDaisy#535).
- Robustly seek to the WAV 'data' chunk to avoid header bytes playing as audio.
- No fades or ramps; audio plays exactly as in the file.

What changed
- New player: `SdramWavPlayer.h/.cpp` uses an SDRAM ring buffer with two halves.
- On init and file switch: seeks to 'data' and pre-fills both halves so the first boundary is smooth.
- Audio callback reads from SDRAM; `Prepare()` (called in the main loop) refills the half that was just consumed.
- `WavPlayer.cpp` defines a small SDRAM buffer and mirrors mono to L/R.

File overview
- `SdramWavPlayer.h/.cpp` — local SDRAM-buffered player
- `WavPlayer.cpp` — example using DaisyPod + FatFS + SDRAM player
- `Makefile` — adds SdramWavPlayer.cpp and enables FatFS

Usage hints
- SD card must be FAT/FAT32 and mounted (handled in example).
- Default buffer: 4096 int16 samples (8 KB). You can raise `SDRAM_BUF_SAMPS` to reduce SD I/O frequency (e.g., 16384).
  - Keep half-buffer size a multiple of 512 bytes for best SD performance. With int16 samples, half-size in samples should be a multiple of 256.
- Format: expects 16-bit PCM WAV. Example outputs mono to L/R. Stereo/24-bit parsing isn’t implemented here.
- Looping is enabled in the example; toggle via `SetLooping` if needed.

Notes
- Start pops were removed by correct 'data' chunk seeking and by priming both halves before first playback.
- We did not add per-playback fades to preserve original audio.

Credits
- Based on DaisyExamples WavPlayer; workaround inspired by libDaisy issue #535.
