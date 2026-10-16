# Admin consolidation — 2026-10-16

The active dashboard is https://onestep-auth.gogogamesas.workers.dev/admin.
The legacy `wolf-auth-api` root, `/admin` and `/admin/` now redirect GET/HEAD
navigation there. Query strings and credentials are not forwarded.

The legacy API implementation and its bindings were preserved, not replaced
with the current Onestep API. Cloudflare deployment
`04ee59c59b16473daa841247e3b2a99a` adds a small entry module which calls
`redirectLegacyAdmin` from `legacy-admin-redirect.mjs`, then delegates all other
requests to the original `worker.js` default handler. It re-exports the original
`RateLimiter`. The original script remains in Cloudflare version history.

At the owner's request, legacy database
`d2da3fa4-c02c-4928-bb31-9a29826f0e4c` license IDs 13–22 were audited and deleted
in a batch. Zero legacy licenses remained afterwards. Public configs, users,
auth logs, audit history and the new Onestep database were not deleted.

Before deletion, all ten complete license rows were encrypted into the ignored
file `.credentials/legacy-licenses-2026-10-16.enc.json`. Decryption was verified
before deleting. This snapshot uses the existing RSA wrapping key protected by
Windows DPAPI. Preserve `.credentials/wrapping-key.dpapi` and the Windows account;
the encrypted file alone is not sufficient for recovery.

For an explicit recovery, `Read-OnestepAuthVault -EnvelopePath <snapshot>` in
`scripts/AuthVault.ps1` returns the database identifier and rows. Do not print
them or automatically restore revoked licenses; restoring rows reauthorizes
those licenses. The current admin credential and owner license remain in the
original vault, unchanged. `scripts/Copy-AuthCredential.ps1 -Kind Admin` copies
the admin credential to the clipboard without writing plaintext to disk.

`wrangler.legacy.toml` is historical reference only. Do not deploy the current
`worker.js` against the legacy database using that configuration.
