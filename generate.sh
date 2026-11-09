#!/usr/bin/env bash
#
# Regenerates the test media and the manifest.
#
# The checked-in media and manifest.json are the canonical fixtures. This
# script is provided so the content can be reproduced and audited; a different
# FFmpeg build may produce different bytes, in which case both the media and
# the manifest must be regenerated together.
#
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p media

common_video=(
    -map_metadata -1
    -fflags +bitexact
    -flags +bitexact
    -c:v libx264
    -pix_fmt yuv420p
    -g 25
    -keyint_min 25
    -crf 26
    -movflags +faststart
)

ffmpeg -y -hide_banner -loglevel error \
    -f lavfi -i "testsrc2=size=320x240:rate=25:duration=2" \
    "${common_video[@]}" -profile:v baseline -bf 0 \
    media/h264-baseline-320x240-25fps.mp4

ffmpeg -y -hide_banner -loglevel error \
    -f lavfi -i "testsrc2=size=320x240:rate=25:duration=2" \
    "${common_video[@]}" -profile:v main -bf 3 \
    media/h264-bframes-320x240-25fps.mp4

ffmpeg -y -hide_banner -loglevel error \
    -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=2" \
    -map_metadata -1 -fflags +bitexact -flags +bitexact \
    -ac 2 -c:a aac -b:a 96k \
    media/aac-tone-48khz-stereo.m4a

ffmpeg -y -hide_banner -loglevel error \
    -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=2" \
    -map_metadata -1 -fflags +bitexact -flags +bitexact \
    -ac 2 -c:a pcm_s16le \
    media/pcm-s16le-tone-48khz-stereo.wav

python3 - <<'PY'
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent
media = sorted((root / "media").glob("*"))

entries = []
for path in media:
    probe = subprocess.run(
        [
            "ffprobe", "-v", "error",
            "-count_packets", "-count_frames",
            "-show_entries",
            "stream=index,codec_type,codec_name,width,height,sample_rate,channels:"
            "stream=nb_read_packets,nb_read_frames,duration",
            "-of", "json", str(path),
        ],
        check=True, capture_output=True, text=True,
    )
    streams = []
    for stream in json.loads(probe.stdout)["streams"]:
        item = {
            "index": int(stream["index"]),
            "type": stream["codec_type"],
            "codec": stream["codec_name"],
        }
        if stream["codec_type"] == "video":
            item["width"] = int(stream["width"])
            item["height"] = int(stream["height"])
        if stream["codec_type"] == "audio":
            item["sample_rate"] = int(stream["sample_rate"])
            item["channels"] = int(stream["channels"])
        if "nb_read_packets" in stream:
            item["packets"] = int(stream["nb_read_packets"])
        if "nb_read_frames" in stream:
            item["frames"] = int(stream["nb_read_frames"])
        streams.append(item)

    entries.append({
        "file": str(path.relative_to(root)),
        "size": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "streams": streams,
    })

manifest = {
    "version": 1,
    "description": "Fixtures for the avcpp codec send/receive tests",
    "media": entries,
}
(root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
PY

echo "Generated $(find media -type f | wc -l) files"
