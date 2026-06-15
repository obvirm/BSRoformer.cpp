#!/usr/bin/env python3
import argparse
import array
import hashlib
import json
import math
import struct
import sys
from pathlib import Path


WAVE_FORMAT_PCM = 0x0001
WAVE_FORMAT_IEEE_FLOAT = 0x0003
WAVE_FORMAT_EXTENSIBLE = 0xFFFE
KSDATAFORMAT_SUBTYPE_PCM = bytes.fromhex("0100000000001000800000aa00389b71")
KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = bytes.fromhex("0300000000001000800000aa00389b71")


def read_chunks(path):
    data = Path(path).read_bytes()
    if len(data) < 12 or data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"not a RIFF/WAVE file: {path}")

    offset = 12
    chunks = {}
    while offset + 8 <= len(data):
        chunk_id = data[offset:offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        start = offset + 8
        end = start + size
        if end > len(data):
            raise ValueError(f"truncated chunk {chunk_id!r} in {path}")
        chunks.setdefault(chunk_id, []).append(data[start:end])
        offset = end + (size & 1)
    return chunks


def read_wav_f32(path):
    chunks = read_chunks(path)
    if b"fmt " not in chunks or b"data" not in chunks:
        raise ValueError(f"missing fmt or data chunk: {path}")

    fmt = chunks[b"fmt "][0]
    if len(fmt) < 16:
        raise ValueError(f"invalid fmt chunk: {path}")

    audio_format, channels, sample_rate, _byte_rate, _block_align, bits_per_sample = struct.unpack_from("<HHIIHH", fmt, 0)
    if audio_format == WAVE_FORMAT_EXTENSIBLE:
        if len(fmt) < 40:
            raise ValueError(f"invalid extensible fmt chunk: {path}")
        sub_format = fmt[24:40]
        if sub_format == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT:
            audio_format = WAVE_FORMAT_IEEE_FLOAT
        elif sub_format == KSDATAFORMAT_SUBTYPE_PCM:
            audio_format = WAVE_FORMAT_PCM

    raw = chunks[b"data"][0]
    if audio_format == WAVE_FORMAT_IEEE_FLOAT and bits_per_sample == 32:
        samples = array.array("f")
        samples.frombytes(raw)
        if sys.byteorder != "little":
            samples.byteswap()
        return channels, sample_rate, samples

    if audio_format != WAVE_FORMAT_PCM:
        raise ValueError(f"unsupported WAV format {audio_format} / {bits_per_sample} bits: {path}")

    if bits_per_sample == 16:
        ints = array.array("h")
        ints.frombytes(raw)
        if sys.byteorder != "little":
            ints.byteswap()
        scale = 32768.0
        return channels, sample_rate, array.array("f", (x / scale for x in ints))

    if bits_per_sample == 32:
        ints = array.array("i")
        ints.frombytes(raw)
        if sys.byteorder != "little":
            ints.byteswap()
        scale = 2147483648.0
        return channels, sample_rate, array.array("f", (x / scale for x in ints))

    raise ValueError(f"unsupported PCM bit depth {bits_per_sample}: {path}")


def file_sha256(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def compare(ref_path, candidate_path):
    ref_channels, ref_rate, ref = read_wav_f32(ref_path)
    cand_channels, cand_rate, cand = read_wav_f32(candidate_path)
    if ref_channels != cand_channels or ref_rate != cand_rate:
        raise ValueError(
            f"metadata mismatch: ref channels/rate={ref_channels}/{ref_rate}, "
            f"candidate channels/rate={cand_channels}/{cand_rate}"
        )
    if len(ref) != len(cand):
        raise ValueError(f"sample count mismatch: ref={len(ref)}, candidate={len(cand)}")

    sum_sq = 0.0
    sum_abs = 0.0
    ref_sum_sq = 0.0
    max_abs = 0.0
    ref_peak = 0.0
    candidate_peak = 0.0
    non_finite_ref = 0
    non_finite_candidate = 0
    for a, b in zip(ref, cand):
        a = float(a)
        b = float(b)
        if not math.isfinite(a):
            non_finite_ref += 1
        if not math.isfinite(b):
            non_finite_candidate += 1
        ref_peak = max(ref_peak, abs(a))
        candidate_peak = max(candidate_peak, abs(b))
        diff = a - b
        abs_diff = abs(diff)
        if abs_diff > max_abs:
            max_abs = abs_diff
        sum_abs += abs_diff
        sum_sq += diff * diff
        ref_sum_sq += a * a

    rms = math.sqrt(sum_sq / len(ref)) if ref else 0.0
    mean_abs = sum_abs / len(ref) if ref else 0.0
    ref_rms = math.sqrt(ref_sum_sq / len(ref)) if ref else 0.0
    if rms == 0.0:
        snr_db = 999.0
    elif ref_rms > 0.0:
        snr_db = 20.0 * math.log10(ref_rms / rms)
    else:
        snr_db = -999.0
    return {
        "reference": str(Path(ref_path).resolve()),
        "candidate": str(Path(candidate_path).resolve()),
        "reference_sha256": file_sha256(ref_path),
        "candidate_sha256": file_sha256(candidate_path),
        "channels": ref_channels,
        "sample_rate": ref_rate,
        "samples": len(ref),
        "rms": rms,
        "mean_abs": mean_abs,
        "max_abs": max_abs,
        "snr_db": snr_db,
        "reference_peak": ref_peak,
        "candidate_peak": candidate_peak,
        "non_finite_reference": non_finite_ref,
        "non_finite_candidate": non_finite_candidate,
        "has_non_finite": non_finite_ref > 0 or non_finite_candidate > 0,
    }


def main():
    parser = argparse.ArgumentParser(description="Compare two WAV files with RMS, SNR, peak, and hash metrics.")
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--rms-threshold", type=float, default=None)
    parser.add_argument("--max-abs-threshold", type=float, default=None)
    parser.add_argument("--snr-threshold", type=float, default=None)
    parser.add_argument("--fail-on-non-finite", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    result = compare(args.reference, args.candidate)
    text = json.dumps(result, indent=2)
    if args.json:
        print(text)
    else:
        print(
            "rms={:.9g} mean_abs={:.9g} max_abs={:.9g} snr_db={:.5g} "
            "ref_peak={:.9g} cand_peak={:.9g} non_finite_ref={} non_finite_candidate={} samples={}".format(
                result["rms"],
                result["mean_abs"],
                result["max_abs"],
                result["snr_db"],
                result["reference_peak"],
                result["candidate_peak"],
                result["non_finite_reference"],
                result["non_finite_candidate"],
                result["samples"],
            )
        )

    failed = False
    if args.rms_threshold is not None and result["rms"] > args.rms_threshold:
        failed = True
    if args.max_abs_threshold is not None and result["max_abs"] > args.max_abs_threshold:
        failed = True
    if args.snr_threshold is not None and result["snr_db"] < args.snr_threshold:
        failed = True
    if args.fail_on_non_finite and result["has_non_finite"]:
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
