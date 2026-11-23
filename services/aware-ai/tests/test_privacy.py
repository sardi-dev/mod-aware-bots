from aware_ai.privacy import clean


def test_clean_redacts_addresses_but_keeps_clock_times() -> None:
    text = "pull at 10:30:45, mail me a@b.example from 192.0.2.20 or fe80:0:0:0:200:f8ff:fe21:67cf"
    assert clean(text) == "pull at 10:30:45, mail me [redacted] from [redacted] or [redacted]"


def test_clean_drops_sensitive_keys_and_the_key() -> None:
    assert clean({"account_name": "x", "line": "sk-test hi"}, secret="sk-test") == {"line": "[redacted] hi"}
