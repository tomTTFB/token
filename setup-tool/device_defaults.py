"""Mirrors the account defaults hardcoded in src/account_store.h.

AccountLink's ADD protocol doesn't take digits/period/algorithm -- every
account it creates uses these. Kept as a small standalone module (rather
than parsed out of the C++ header) since there's nothing to parse from
Python; just keep the two values in sync by hand if account_store.h's
DEFAULT_DIGITS/DEFAULT_PERIOD ever change.
"""

DEFAULT_DIGITS = 6
DEFAULT_PERIOD = 30
