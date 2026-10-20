import {readFileSync} from 'node:fs';
// Public configuration only. Never opens a local vault or reads a secret value.
const config=JSON.parse(readFileSync(new URL('../wrangler.jsonc',import.meta.url),'utf8').replace(/^\s*\/\/.*$/gm,''));
const vars=config.vars || {};
if (vars.AUTH_V2_ONLY !== 'true' || vars.AUTH_V2_RELEASES !== 'true') {
    console.error('Deployment blocked: production must keep launcher-only v2 and the D1 release registry enabled.');
    process.exit(1);
}
if (!/^https:\/\/[a-z0-9][a-z0-9-]{0,62}\.cloudflareaccess\.com$/.test(vars.ADMIN_ACCESS_TEAM_DOMAIN || '') ||
    !/^[a-f0-9]{64}$/i.test(vars.ADMIN_ACCESS_AUD || '') || !vars.ADMIN_ACCESS_EMAIL) {
    console.error('Deployment blocked: enable Cloudflare Access for BOTH /admin* and /api/v1/admin/*; configure ADMIN_ACCESS_TEAM_DOMAIN, ADMIN_ACCESS_AUD and ADMIN_ACCESS_EMAIL first. See AUTH_HARDENING.md.');
    process.exitCode=1;
} else console.log('Public admin Access configuration present. Complete the migration/rotation checklist before deployment.');
