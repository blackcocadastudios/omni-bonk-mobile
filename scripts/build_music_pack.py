#!/usr/bin/env python3
"""Builds an optional, local-only music pack for Omni Bonk from audio files you own.

The game looks for menu/forest/desert/graveyard/boss/final/frostpeak/caldera.ogg in its music folder and plays them instead of the
built-in chiptune (a missing slot keeps the chiptune). The pack is never part of the repository, the CI or the
public releases: keep it on your own devices.

  python3 scripts/build_music_pack.py --out music --menu calm.mp3 --forest a.flac --boss b.ogg ...

Each track is trimmed of leading/trailing silence, loudness-normalized (two-pass EBU R128) so it sits at the level
of the game's sound effects, resampled to 48 kHz and encoded as OGG Vorbis. Requires ffmpeg with libvorbis.

Where the folder goes:
  R36S / PortMaster : ports/omnibonk/music/            (next to omnibonk.aarch64)
  Android           : Android/data/com.blackcocada.omnibonk/files/music/   (USB file transfer or adb push)
  Linux desktop     : ./music or --music DIR
"""
import argparse, json, os, re, shutil, subprocess, sys

SLOTS = ["menu", "forest", "desert", "graveyard", "boss", "final", "frostpeak", "caldera"]


def ffmpeg(args):
    return subprocess.run(["ffmpeg", "-hide_banner", "-nostdin"] + args, capture_output=True, text=True, encoding="utf8", errors="replace")


def build(src, dst, lufs, rate, mono, quality):
    trim = "silenceremove=start_periods=1:start_threshold=-55dB,areverse,silenceremove=start_periods=1:start_threshold=-55dB,areverse"
    norm = f"loudnorm=I={lufs}:TP=-1.5:LRA=11"
    r = ffmpeg(["-i", src, "-af", f"{trim},{norm}:print_format=json", "-f", "null", "-"])
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", r.stderr)
    if r.returncode or not m:
        sys.exit(f"ffmpeg could not analyse {src}:\n{r.stderr[-800:]}")
    s = json.loads(m.group(0))
    second = (f"{norm}:measured_I={s['input_i']}:measured_TP={s['input_tp']}:measured_LRA={s['input_lra']}"
              f":measured_thresh={s['input_thresh']}:offset={s['target_offset']}:linear=true")
    fade_in = "afade=t=in:d=0.02"
    r = ffmpeg(["-y", "-i", src, "-vn", "-map_metadata", "-1", "-af", f"{trim},{second},aresample={rate},{fade_in}",
                "-ac", "1" if mono else "2", "-c:a", "libvorbis", "-q:a", str(quality), dst])
    if r.returncode:
        sys.exit(f"ffmpeg failed on {src}:\n{r.stderr[-800:]}")
    return float(s["input_i"])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="output folder (created if missing)")
    for slot in SLOTS:
        ap.add_argument(f"--{slot}", metavar="FILE", help=f"source for {slot}.ogg")
    ap.add_argument("--lufs", type=float, default=-14.0, help="target integrated loudness (default -14 LUFS)")
    ap.add_argument("--rate", type=int, default=48000, help="sample rate (the game mixes at 48000)")
    ap.add_argument("--mono", action="store_true", help="mono files: half the decoding work (the R36S speaker is mono)")
    ap.add_argument("--quality", type=int, default=4, help="Vorbis quality 0-10 (4 is about 128 kbps)")
    a = ap.parse_args()
    if not shutil.which("ffmpeg"):
        sys.exit("ffmpeg not found in PATH")
    jobs = [(slot, getattr(a, slot)) for slot in SLOTS if getattr(a, slot)]
    if not jobs:
        sys.exit("nothing to do: give at least one of " + ", ".join("--" + s for s in SLOTS))
    os.makedirs(a.out, exist_ok=True)
    for slot, src in jobs:
        if not os.path.isfile(src):
            sys.exit(f"missing file for --{slot}: {src}")
        dst = os.path.join(a.out, slot + ".ogg")
        before = build(src, dst, a.lufs, a.rate, a.mono, a.quality)
        print(f"{slot:10s} {before:6.1f} LUFS -> {a.lufs:.0f} LUFS  {os.path.getsize(dst) / 1e6:5.2f} MB  <- {os.path.basename(src)}")
    print(f"done: {len(jobs)} track(s) in {os.path.abspath(a.out)}  (slots without a file keep the chiptune)")


if __name__ == "__main__":
    main()
