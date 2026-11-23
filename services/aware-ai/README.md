# aware-ai

Optional AI sidecar for a 5.4.8 (build 18414) private server. It offers PvP decisions and limited player-facing bot chat through OpenRouter; worldserver supplies rule-based fallbacks. Its design follows the optional AI layer pattern of an AzerothCore based server.

## Run and test

With Python 3.12 and the dependencies in `pyproject.toml` already installed, from this directory:

```sh
uvicorn aware_ai.main:create_app --factory --host 0.0.0.0 --port 8090
pytest
ruff check .
ruff format --check .
mypy
python -m aware_ai.export_schemas
```

The Dockerfile also provides runtime and dev targets. Configure data, log and template paths for your deployment. The sidecar must be accessible to worldserver at `http://aware-ai:8090`. **It has no authentication**: anyone who can reach the port can change its settings and spend your provider budget, so keep it on a private container network with worldserver and never publish port 8090. Tests mock OpenRouter and do not use the network. JSON Schemas for the v1 API are in `schemas/v1/`.

## Environment

| Variable | Default | Purpose |
|---|---|---|
| `OPENROUTER_API_KEY` | unset | Provider key, never intentionally logged |
| `OPENROUTER_BASE_URL` | OpenRouter API v1 | Provider endpoint |
| `AWARE_AI_DATA_DIR`, `AWARE_AI_LOG_DIR`, `AWARE_AI_TEMPLATE_DIR` | `/data`, `/logs`, `/app/templates` | Storage, JSONL logs, templates |
| `AWARE_AI_JEV_MODEL` | `typesafe/jev-1.14` | Decision model |
| `AWARE_AI_JEV_USD_PER_MTOK` | `0.042` | Decision input token fallback price |
| `AWARE_AI_MONTHLY_CEILING_USD` | `20` | Maximum effective monthly settings cap |
| `AWARE_AI_DAY_ALARM_FRACTION` | `0.25` | Warn once per UTC day when spend passes this fraction of the monthly cap |
| `AWARE_AI_CHAT_CONVERSATION_PER_MIN` | `4` | Per bot and human conversation rate |
| `AWARE_AI_CHAT_BOT_INTERVAL_S`, `AWARE_AI_CHAT_HUMAN_PER_MINUTE` | `30`, `6` | Chat rate limits |
| `AWARE_AI_GUILD_RECENT_S` | `60` | Maximum age of a human guild message |
| `AWARE_AI_CHAT_MODEL` | unset | Chat model ID; unset disables live AI chat. This server uses `deepseek/deepseek-v4-flash-0731` (A/B tests of 2026-11-17) |
| `AWARE_AI_CHAT_MAX_TOKENS`, `AWARE_AI_CHAT_TEMPERATURE` | `120`, `0.8` | Chat generation parameters |
| `AWARE_AI_CHAT_ZDR` | `1` | Require zero-data-retention routing for chat |
| `AWARE_AI_CHAT_USD_PER_MTOK_IN`, `AWARE_AI_CHAT_USD_PER_MTOK_OUT` | `1`, `4` | Chat fallback token prices |
| `AWARE_AI_FAKE` | unset | With no key, return deterministic decisions and canned chat, uncharged |
| `AWARE_AI_LOG_RETENTION_DAYS` | `90` | JSONL retention |
| `AWARE_AI_MAX_IN_FLIGHT`, `AWARE_AI_REQUEST_TIMEOUT_S` | `16`, `2` | Provider concurrency and decision timeout |
| `AWARE_AI_BREAKER_FAILURES`, `AWARE_AI_BREAKER_OPEN_S` | `5`, `60` | Circuit breaker threshold and cooldown |

## Budgets and privacy

Worldserver pushes the enabled flag, features, monthly budget and optional daily budget through `PUT /v1/settings`. The effective monthly cap is the lower of the pushed cap and the environment ceiling. A null daily cap disables the daily limit; a number sets an optional shared daily cap. Both decision and chat spend count against the shared monthly pool. Daily spend above the alarm fraction logs a warning and appears in status without blocking calls. Periods use UTC. The ledger records `usage.cost`, falling back to configured token prices when absent. Charges are only known after a provider response, so while a call runs the pool sets aside an estimate of its cost (a decision's from the size of its prompt, a chat reply's from its prompt and `AWARE_AI_CHAT_MAX_TOKENS`); a call is refused with `budget_exhausted` when what is spent plus what running calls may still cost reaches a cap. Concurrent calls therefore overshoot a cap by one estimate at most, not by one call per slot. Set a provider-side key limit as a backstop.

Requests should contain game context only. Unknown chat fields are rejected; obvious sensitive field names, email addresses and IP addresses are stripped from flexible decision state and text before provider calls and logs. Automated filtering cannot identify every account name or sensitive detail typed in free text, so callers must enforce the game-only policy. JSONL logs retain prompt context. Never put secrets into game context.

## Decision templates

Templates in `templates/*.toml` hold the prompt and the fixed option names of one `decision_type`; `GET /v1/status` lists them. `pvp_engage` and `pvp_reassess` are duel decisions. `bg_strategy` is the battleground leader's ask (about every 20 s per side, a stance, floater job, wipe reply or target node): its options are the fixed slots `option_0` to `option_5`, and the request's `state.candidates` says what each slot means for that request, so the prompt stays the same and a cached answer is only reused for the same candidates. The model returns a choice with probabilities and no text; with `reason_from_probabilities` the response's `reason` is the margin of the choice (`option_1 0.72, next option_0 0.20`). A request is about 900 to 1,150 input tokens (the glossary of the candidates is most of it), a few hundredths of a cent at the listed price; the worldserver caps one side at 60 asks a match.

## Chat and fake mode

Chat is disabled until enabled by settings. Live chat also requires a configured model and key, and sends reasoning effort `none`. Kinds are `duel_challenge`, `duel_win`, `duel_loss`, `bg_win`, `bg_loss`, `bg_capture`, `whisper_reply`, `nearby_reply`, `party_moment`, and `guild_reply`. Guild replies require `guild_human_message_age_s` no greater than `AWARE_AI_GUILD_RECENT_S`. Rate-limited requests return `rate_limited` for rule fallback. Fake mode works without a key and does not require a chat model; without a key or fake mode the reason is `no_api_key`.

## Model comparison

Set `OPENROUTER_API_KEY` in the environment and run `python tools/chat_ab.py --output results [model-id ...]`. It sends the same 30 prompts from `tools/chat_prompts.json` to each model and writes `comparison.md` and `results.json` with replies, latency, tokens and reported cost. This makes live, billable provider requests. The key is read from the environment and is not printed.

Social memory uses a friendly threshold of 20 on a score clamped to -100 through 100. Event weights: grouped +2, dungeon_done +10, boss_killed_together +5, wipe_together +2, duel_won 0, duel_lost -5, loot_ninja -25, loot_passed +10, helped +10, insulted -10, thanked +3. Friend whispers have a 12-hour per-pair cooldown. Quiet takes precedence over rough, then chatty, then friendly.

## Liking score and wipe

Worldserver keeps one liking score per bot and human (design decision 0033 point 14). A chat request may carry it as two optional fields: `liking` (0 to 1) and `relation` (`cold`, `stranger`, `friendly` or `close`, the band worldserver computed with its own edges). When present they are the single source of truth for how the bot feels: the prompt carries the band word and the whole percent only ("Your feeling for them: friendly, 62 of 100."), and a `friend_whisper` goes ahead only for `friendly` or `close`. `relation` wins over `liking`; with `liking` alone the band comes from the default edges 0.20, 0.55 and 0.75. A request without them falls back to the sidecar's own pair score (the 20 threshold above), so the sidecar still works when worldserver does not send them. The event counts ("run dungeons with them 3 time(s)") stay in the prompt either way: they are facts, not the score.

`POST /v1/memory/wipe` with `{"schema_version": 1, "player_guid": N}` deletes every `memory_event`, `chat_turn`, `social_event` and `pair_note` row of that human, the lines of the JSONL logs that name them (`player_guid` or `target_guid`) and their in-memory rate and cooldown entries, and answers the counts. `{"schema_version": 1, "all": true}` does it for every human. The spend ledger and settings are never touched. Worldserver calls it from its `.playerbots social wipe` command when the AI layer is on.

## License

GPL-2.0-or-later, like the rest of this repository (see `LICENSE` and `NOTICE.md` at the root). The sidecar contains no provider key; set `OPENROUTER_API_KEY` in the environment.

## Admitting to being a bot

A chat request may carry `deny_being_bot` (boolean, default false). The prompt template `templates/chat/system.toml` holds two sentences and the sidecar picks one: with the flag the bot is told to deny being a bot playfully, without it (the default) the bot says plainly that it is one when a player asks. Worldserver sends the flag only while `AwareAi.Chat.DenyBeingBot` is 1 (default 0). A chat template that lacks `identity_denying` or `identity_open` is refused at start-up.
