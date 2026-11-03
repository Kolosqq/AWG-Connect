import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import urllib.request
import zipfile


RELEASE_URL = (
    "https://github.com/amnezia-vpn/amnezia-client/releases/download/5.1.0.6/"
    "AmneziaVPN_5.1.0.6_android11%2B_arm64-v8a.apk"
)
ARCHIVE_SHA256 = "27a937655e0c50a90e0dba1316244914f95a361185cbf446d0e897424a64059d"
FIELD_SHA256 = {
    "PROD_AGW_PUBLIC_KEY": "8c470f207d071799be894e5562a171da73400268a195a32106e7fd56fe30bc36",
    "DEV_AGW_PUBLIC_KEY": "b38cb7b96272227536369938f36bd42386dec43b92022e05036a060ab5687f19",
    "PROD_S3_ENDPOINT": "c4d9ea102ddefc471c6bd4586426e270e0e95ad523de207e66d9c0bfda489103",
    "DEV_S3_ENDPOINT": "176e763fbeea9938687cc304f60973709d8cccabbd872e7b639e742b242ca9e2",
    "FALLBACK_S3_ENDPOINT": "b13f50bbd133b179e1d345563ac18dbcfc621320603694fe929c996a1d82ee05",
}


def public_settings(archive_data):
    if hashlib.sha256(archive_data).hexdigest() != ARCHIVE_SHA256:
        raise ValueError("Official package SHA-256 mismatch")
    with zipfile.ZipFile(io.BytesIO(archive_data)) as archive:
        library = archive.read("lib/arm64-v8a/libAmneziaVPN_arm64-v8a.so")
    candidates = {
        hashlib.sha256(value).hexdigest(): value.decode("ascii")
        for value in re.findall(rb"[\x20-\x7e\r\n]{12,}", library)
    }
    return {name: candidates[digest] for name, digest in FIELD_SHA256.items()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--github-env", type=Path, default=os.environ.get("GITHUB_ENV"))
    args = parser.parse_args()
    if args.archive:
        data = args.archive.read_bytes()
    else:
        with urllib.request.urlopen(RELEASE_URL, timeout=120) as response:
            data = response.read()
    settings = public_settings(data)
    if args.github_env:
        with args.github_env.open("a", encoding="utf-8") as output:
            for name, value in settings.items():
                escaped = value.replace("\r", "").replace("\n", r"\n")
                output.write(f"{name}={escaped}\n")
    print("Verified official public gateway parameters:", ", ".join(settings))


if __name__ == "__main__":
    main()
