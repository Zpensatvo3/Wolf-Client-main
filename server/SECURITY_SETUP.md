# Wolf Auth production setup

The Worker intentionally fails closed until every secret and the Durable Object
binding are configured. Do not restore source-code fallbacks.

1. Deploy `schema.sql` only as a safe bootstrap/migration. It does not create a
   demo license.
2. Copy `wrangler.toml.example` to your private `wrangler.toml`, set the D1 id,
   then deploy the `RATE_LIMITER` Durable Object migration.
3. Generate random `KEY_PEPPER`, `HWID_PEPPER`, and a long random admin token.
   Store their values as Worker secrets. Store only `SHA-256(admin-token)` in
   `ADMIN_TOKEN_HASH`.
4. Generate an ECDSA P-256 key pair in binary DER form. Store the base64url
   PKCS#8 private key as `SIGNING_PRIVATE_KEY_PKCS8` and its base64url SPKI
   public key as `SIGNING_PUBLIC_KEY_SPKI`. Do not base64-encode PEM text.
5. Rebuild the DLL with the exact same public SPKI value:

   ```powershell
   msbuild custom-base.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:AuthPublicKeySpkiBase64=<base64url-spki>
   ```

The private key, peppers, and admin token must never be placed in this
repository, the DLL, a URL, or browser storage. Rotating the signing key
requires updating the Worker secrets and rebuilding the DLL with the new public
key.

Changing `KEY_PEPPER` changes every license hash. Before rotating it on an
existing installation, run a controlled server-side migration using the old
pepper, or reissue the affected licenses; otherwise existing keys will no
longer authenticate.

New dashboard-created licenses are stored encrypted at rest with a key derived
from `KEY_PEPPER`; an authenticated admin can reveal one from the dashboard.
Older records that were already stored as one-way hashes cannot be recovered
and must be reissued if their original key was not saved elsewhere.
