"""Encode a recording to 16 kHz mono Opus, 24 kbit/s CBR, 20 ms packets.

Requires FFmpeg with libopus and ffprobe. Runs offline, never on the device.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from opus_packets import pack_packets, packet_samples


def run(command):
    return subprocess.run(command, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--alaw-source", action="store_true", help="Source is raw 8 kHz mono A-law")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    args = parser.parse_args()
    manifest = args.output.with_suffix(args.output.suffix + ".json")
    if args.output.exists() or manifest.exists():
        parser.error("Output or manifest already exists; use a new path")
    with tempfile.TemporaryDirectory() as temp:
        ogg = str(Path(temp) / "encoded.opus")
        command = [args.ffmpeg, "-v", "error", "-nostdin"]
        if args.alaw_source:
            command += ["-f", "alaw", "-ar", "8000", "-ac", "1"]
        command += ["-i", str(args.source), "-vn", "-ar", "16000", "-ac", "1",
                    "-c:a", "libopus", "-application", "voip", "-b:a", "24k", "-vbr", "off",
                    "-frame_duration", "20", ogg]
        run(command)
        metadata = json.loads(run([args.ffprobe, "-v", "error", "-select_streams", "a:0",
                                   "-show_packets", "-show_entries", "packet=size", "-of", "json", ogg]))
        raw = run([args.ffmpeg, "-v", "error", "-nostdin", "-i", ogg, "-map", "0:a:0",
                   "-c:a", "copy", "-f", "data", "pipe:1"])
        packets, offset = [], 0
        for item in metadata["packets"]:
            size = int(item["size"])
            packet = raw[offset:offset + size]
            if size != 60 or len(packet) != size or packet_samples(packet) != 960:
                raise ValueError("Unexpected encoding; expected 60-byte mono 20 ms Opus packets")
            packets.append(packet)
            offset += size
        if not packets or offset != len(raw):
            raise ValueError("Demuxed packet boundaries do not match")
        encoded = pack_packets(packets)
        manifest_data = {"codec": "opus", "sample_rate": 16000, "channels": 1,
                         "bitrate": 24000, "packet_ms": 20, "packet_count": len(packets),
                         "payload_bytes": 60, "storage": "uint16le length + raw Opus packet",
                         "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
                         "sha256": hashlib.sha256(encoded).hexdigest(),
                         "encoder": run([args.ffmpeg, "-version"]).decode().splitlines()[0]}
        args.output.write_bytes(encoded)
        manifest.write_text(json.dumps(manifest_data, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote {len(packets)} packets, {len(encoded)} bytes: {args.output}")


if __name__ == "__main__":
    main()
