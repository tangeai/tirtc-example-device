import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from opus_packets import unpack_packets, packet_samples


class AssetContractTests(unittest.TestCase):
    def test_supplied_sdk_matches_provenance(self):
        expected = {
            "lib/libTiRTC.a": "05d37bb0466d262aee634e9a8c0da7e0088410d7a109a840cb854cc2bbb3d73c",
            "include/tiRTC.h": "b1315121abf843c43c669f754382cab302c3b0b35bdfe41f4c4756ef95636293",
            "include/basedef.h": "23883b8040838fcda10927523536def667cfe7be7fcd8b6f4a1c9410d3e03c04",
        }
        lock = ROOT / "components/tirtc_sdk/release-lock.json"
        if lock.exists():
            profile = json.loads(lock.read_text())
            expected["lib/libTiRTC.a"] = profile["sdk_sha256"]
            self.assertEqual(profile["target"], "esp32c3")
            self.assertEqual(profile["idf_version"], "5.5.5")
        for name, digest in expected.items():
            with self.subTest(file=name):
                actual = hashlib.sha256((ROOT / "components/tirtc_sdk" / name).read_bytes()).hexdigest()
                self.assertEqual(actual, digest)

    def test_opus_asset(self):
        asset = ROOT / "components/wechat_voip/assets/audio.opuspkt"
        data = asset.read_bytes()
        manifest = json.loads(asset.with_suffix(".opuspkt.json").read_text())
        packets = unpack_packets(data)
        self.assertEqual(hashlib.sha256(data).hexdigest(), manifest["sha256"])
        self.assertEqual(manifest["source_sha256"],
                         "807305ac8e76212b271c5de21d4fc082e08fa369e42b20dd44363a895a3e2d4f")
        self.assertEqual(manifest["codec"], "opus")
        self.assertEqual(manifest["sample_rate"], 16000)
        self.assertEqual(manifest["packet_count"], len(packets))
        self.assertEqual(len(packets), 501)
        for packet in packets:
            self.assertEqual(len(packet), 60)
            self.assertEqual(packet_samples(packet), 960)
