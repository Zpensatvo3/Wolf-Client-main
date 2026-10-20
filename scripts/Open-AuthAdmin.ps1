#requires -Version 7.0
# Opening the browser must not decrypt production secrets first.
Start-Process 'https://onestep-auth.gogogamesas.workers.dev/admin' -WindowStyle Hidden
