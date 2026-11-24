"""Python counterpart of test-xdev-hb.prg; same algorithm and output fields.

Run with CPython: python test-xdev-py.py 1000000
Without an argument the limit is 500000.

Measured on 2026-11-06: Intel Core i7-13620H, Windows 10 Pro, 64-bit.
xDev 0.0.0 (r2026-11-06 10:35), Harbour 3.2.1dev (r2609180937),
Clang 22.1.8, CPython 3.13.11 (MSYS2 CLANG64).
Limit: 1000000. One warm-up, then five sequential runs per variant,
rotating their order. Median computation times, excluding startup/output:
    xDev                         0.420 s
    Harbour C (-gc3, -O2)        1.124 s
    Harbour VM (default)         1.737 s
    CPython 3.13.11              1.423 s
All variants: 78498 primes, sum 37550402023; independently checked by sieve.
Python uses perf_counter(); xDev Seconds() resolution is 0.01 s.
Median whole-process times measured with the same external clock:
0.423 / 1.130 / 1.742 / 1.443 s respectively.
This measures one integer workload, not overall language performance.
"""

import sys
from time import perf_counter


def is_prime(n):
    divisor = 3
    if n == 2:
        return True
    if n % 2 == 0:
        return False
    while divisor * divisor <= n:
        if n % divisor == 0:
            return False
        divisor += 2
    return True


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 500_000
    if limit < 0 or limit > 10_000_000:
        print("Use a limit from 0 to 10000000.")
        return

    count = 0
    total = 0
    start = perf_counter()
    for n in range(2, limit + 1):
        if is_prime(n):
            count += 1
            total += n
    elapsed = perf_counter() - start

    print(f"Limit: {limit}")
    print(f"Primes: {count}")
    print(f"Sum: {total}")
    print(f"Seconds: {elapsed:.6f}")


if __name__ == "__main__":
    main()
