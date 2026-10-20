#requires -Version 7.0
param([ValidateSet('Admin', 'License')][string]$Kind = 'Admin')
throw 'Automatic project-vault credential copying is disabled. Use your separate password manager; admin access also requires Cloudflare Access.'
