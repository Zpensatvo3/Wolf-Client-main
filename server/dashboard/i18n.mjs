// All product copy lives here; the browser retains only the selected locale.
export const messages = {
    tr: {
        admin: 'Yönetim merkezi', workspace: 'Çalışma alanı', overview: 'Genel bakış', licenses: 'Lisanslar',
        activity: 'İşlem geçmişi', system: 'Sistem durumu', settings: 'Tercihler', logout: 'Çıkış yap',
        loginTitle: 'Kontrol sizde.', loginText: 'OneStep yönetim merkezine hoş geldiniz. Devam etmek için yönetici anahtarınızı girin.',
        token: 'Yönetici anahtarı', signIn: 'Güvenli giriş yap', verifying: 'Doğrulanıyor…', tokenHelp: 'Anahtarınız yalnızca bu sekmenin belleğinde tutulur.',
        tokenRequired: '32–512 karakterlik yönetici anahtarını girin.', loginError: 'Giriş doğrulanamadı.',
        eyebrow: 'ONE STEP AHEAD', overviewTitle: 'Her şey bir bakışta.', overviewText: 'Lisanslarınız, erişimleriniz ve son yönetim işlemleriniz.',
        licensesText: 'Her lisansın durumunu ve cihaz bağlantısını yönetin.', activityText: 'Sunucuda kayıtlı yönetim işlemlerinin geçmişi.',
        systemText: 'Bağlantıların ve yayın servisinin son kontrolü.', settingsText: 'Bu yönetim merkezini kendinize göre düzenleyin.',
        refresh: 'Yenile', newLicense: 'Lisans oluştur', total: 'Toplam lisans', active: 'Aktif', unused: 'Kullanılmamış', banned: 'Yasaklı', expired: 'Süresi dolmuş',
        recent: 'Son işlemler', seeAll: 'Tümünü gör', portfolio: 'Erişim dağılımı', portfolioHelp: 'Güncel lisans kayıtlarından hesaplanır.',
        noActivity: 'Henüz kayıtlı işlem yok.', noActivityHelp: 'Yönetim işlemleriniz tamamlandığında burada görünür.',
        search: 'ID, parmak izi veya cihaz ipucu ara', allStatuses: 'Tüm durumlar', newest: 'Önce en yeni', oldest: 'Önce en eski', expirySort: 'Bitiş tarihine göre',
        license: 'Lisans', status: 'Durum', device: 'Cihaz', expiration: 'Geçerlilik', epoch: 'Epoch', details: 'Ayrıntılar',
        bound: 'Bağlı', unbound: 'Bağlanmamış', deviceKey: 'Cihaz anahtarı', lifetime: 'Süresiz', unknown: 'Bilinmiyor',
        noLicenses: 'Eşleşen lisans yok.', noLicensesHelp: 'Filtreleri değiştirin veya yeni bir lisans oluşturun.',
        previous: 'Önceki', next: 'Sonraki', page: 'Sayfa', records: 'kayıt', created: 'Oluşturulma', activated: 'İlk etkinleştirme',
        action: 'İşlem', reason: 'Açıklama', time: 'Zaman', fingerprint: 'Parmak izi', copy: 'Kopyala', copied: 'Panoya kopyalandı.', copyError: 'Kopyalanamadı. Metni seçerek kopyalayabilirsiniz.',
        close: 'Kapat', reveal: 'Anahtarı göster', reset: 'Cihaz bağını sıfırla', ban: 'Lisansı yasakla', unban: 'Yasağı kaldır', delete: 'Lisansı sil',
        cancel: 'Vazgeç', confirm: 'Onayla', confirmTitle: 'Devam etmek istiyor musunuz?',
        revealWarning: 'Tam lisans anahtarı bu sekmede gösterilecek ve işlem geçmişine kaydedilecek.',
        resetWarning: 'HWID ve cihaz anahtarı bağı temizlenecek. Mevcut oturumlar geçersizleşecek; lisans yeniden etkinleştirilebilir olacak. Bu işlem mevcut yasağı da kaldırır.',
        banWarning: 'Lisansın erişimi kapatılacak ve mevcut oturumları geçersizleşecek.',
        unbanWarning: 'Lisansın erişimi yeniden açılacak. Mevcut cihaz bağı ve bitiş tarihi korunacak; önceki oturumlar geçersizleşecek.',
        deleteWarning: 'Lisans kalıcı olarak silinecek ve oturumları geçersizleşecek. İşlem geri alınamaz. Geçmiş işlem kayıtları ve public configler korunur.',
        createWarning: 'Seçilen süreyle yeni bir erişim lisansı oluşturulacak. Süre oluşturulma anından başlar.',
        customKey: 'Özel anahtar (isteğe bağlı)', customHelp: 'Boş bırakırsanız güvenli bir anahtar oluşturulur. 8–128 harf, rakam veya tire.',
        duration: 'Lisans süresi', oneDay: '1 gün', sevenDays: '7 gün', thirtyDays: '30 gün', create: 'Oluştur',
        licenseCreated: 'Lisans oluşturuldu', keyHelp: 'Bu anahtarı güvenli bir yerde saklayın. Ayrıntılardan tekrar gösterebilirsiniz.',
        working: 'İşleniyor…', success: 'İşlem tamamlandı.', error: 'İşlem tamamlanamadı.', loadError: 'Veriler yüklenemedi. Yenilemeyi deneyin.',
        offline: 'Kontrol edilemedi', online: 'Bağlantı hazır', unchecked: 'Henüz kontrol edilmedi', checking: 'Kontrol ediliyor',
        database: 'Lisans veritabanı', authConfig: 'Auth yapılandırması', rateLimit: 'İstek sınırlayıcı', releases: 'Yayın deposu',
        release: 'Güncel sürüm', noRelease: 'Henüz doğrulanmış yayın yok', configured: 'Yapılandırılmış', missing: 'Yapılandırılmamış', checked: 'Son kontrol',
        language: 'Arayüz dili', languageHelp: 'Tercih bu sekmede uygulanır.', session: 'Yönetici oturumu', sessionHelp: 'Anahtar diske kaydedilmez. Sekmeyi yenilemek veya çıkış yapmak oturumu bitirir.',
        apiFailure: 'Sunucu isteği tamamlayamadı.', unauthorized: 'Yönetici anahtarı geçersiz veya oturum kapandı.',
        rateLimited: 'İstek sınırına ulaşıldı. Bir dakika sonra tekrar deneyin.', unavailable: 'Servis şu anda kullanılamıyor.', network: 'Bağlantı kurulamadı veya istek zaman aşımına uğradı.',
        notFound: 'Kayıt bulunamadı. Listeyi yenileyin.', conflict: 'Bu işlem mevcut kayda uygulanamıyor.', invalid: 'İstek alanlarını kontrol edin.',
        revealUnavailable: 'Eski lisans yalnızca hash olarak saklandığından anahtarı gösterilemiyor.',
        CREATE_KEY: 'Lisans oluşturuldu', REVEAL_KEY: 'Anahtar görüntülendi', RESET_HWID: 'Cihaz bağı sıfırlandı', SET_STATUS: 'Durum değiştirildi', DELETE_KEY: 'Lisans silindi'
    },
    en: {
        admin: 'Administration', workspace: 'Workspace', overview: 'Overview', licenses: 'Licenses', activity: 'Activity', system: 'System status', settings: 'Preferences', logout: 'Sign out',
        loginTitle: 'You’re in control.', loginText: 'Welcome to the OneStep administration center. Enter your administrator key to continue.', token: 'Administrator key', signIn: 'Sign in securely', verifying: 'Verifying…', tokenHelp: 'Your key stays in this tab’s memory only.', tokenRequired: 'Enter a 32–512 character administrator key.', loginError: 'Sign-in could not be verified.',
        eyebrow: 'ONE STEP AHEAD', overviewTitle: 'Everything at a glance.', overviewText: 'Your licenses, access, and recent administrative activity.', licensesText: 'Manage license status and device bindings.', activityText: 'Administrative actions recorded by the server.', systemText: 'The latest check of bindings and the release service.', settingsText: 'Make this administration center your own.',
        refresh: 'Refresh', newLicense: 'Create license', total: 'Total licenses', active: 'Active', unused: 'Unused', banned: 'Banned', expired: 'Expired', recent: 'Recent activity', seeAll: 'View all', portfolio: 'Access distribution', portfolioHelp: 'Calculated from current license records.', noActivity: 'No activity yet.', noActivityHelp: 'Completed administrative actions will appear here.',
        search: 'Search ID, fingerprint, or device hint', allStatuses: 'All statuses', newest: 'Newest first', oldest: 'Oldest first', expirySort: 'By expiration', license: 'License', status: 'Status', device: 'Device', expiration: 'Expiration', epoch: 'Epoch', details: 'Details', bound: 'Bound', unbound: 'Unbound', deviceKey: 'Device key', lifetime: 'Lifetime', unknown: 'Unknown', noLicenses: 'No matching licenses.', noLicensesHelp: 'Adjust your filters or create a license.', previous: 'Previous', next: 'Next', page: 'Page', records: 'records', created: 'Created', activated: 'First activation', action: 'Action', reason: 'Description', time: 'Time', fingerprint: 'Fingerprint', copy: 'Copy', copied: 'Copied to clipboard.', copyError: 'Could not copy. Select the text to copy it manually.',
        close: 'Close', reveal: 'Reveal key', reset: 'Reset device binding', ban: 'Ban license', unban: 'Unban license', delete: 'Delete license', cancel: 'Cancel', confirm: 'Confirm', confirmTitle: 'Do you want to continue?', revealWarning: 'The full license key will be shown in this tab and the action will be audited.', resetWarning: 'The HWID and device key bindings will be cleared. Existing sessions will be revoked and the license can be activated again. This also removes an existing ban.', banWarning: 'Access will be blocked and existing sessions will be revoked.', unbanWarning: 'Access will be restored. The device binding and expiration remain. Previous sessions will be revoked.', deleteWarning: 'The license will be permanently deleted and its sessions revoked. This cannot be undone. Audit history and public configs remain.', createWarning: 'A new access license will be created with the selected duration, starting immediately.', customKey: 'Custom key (optional)', customHelp: 'Leave blank to generate a secure key. Use 8–128 letters, digits, or hyphens.', duration: 'License duration', oneDay: '1 day', sevenDays: '7 days', thirtyDays: '30 days', create: 'Create', licenseCreated: 'License created', keyHelp: 'Store this key securely. You can reveal it again in license details.', working: 'Working…', success: 'Action completed.', error: 'Action failed.', loadError: 'Could not load data. Try refreshing.', offline: 'Could not verify', online: 'Connection ready', unchecked: 'Not checked yet', checking: 'Checking', database: 'License database', authConfig: 'Auth configuration', rateLimit: 'Rate limiter', releases: 'Release storage', release: 'Current release', noRelease: 'No verified release published yet', configured: 'Configured', missing: 'Not configured', checked: 'Last checked', language: 'Interface language', languageHelp: 'Applies to this tab.', session: 'Administrator session', sessionHelp: 'Your key is never saved to disk. Refreshing the tab or signing out ends the session.', apiFailure: 'The server could not complete the request.', unauthorized: 'The administrator key is invalid or the session ended.', rateLimited: 'Request limit reached. Try again in a minute.', unavailable: 'The service is currently unavailable.', network: 'Connection failed or the request timed out.', notFound: 'Record not found. Refresh the list.', conflict: 'This action cannot be applied to the current record.', invalid: 'Check the request fields.', revealUnavailable: 'This older license was stored as a hash only and cannot be revealed.', CREATE_KEY: 'License created', REVEAL_KEY: 'Key revealed', RESET_HWID: 'Device binding reset', SET_STATUS: 'Status changed', DELETE_KEY: 'License deleted'
    }
};

// Security policy overrides shared by the initial document and browser rendering.
Object.assign(messages.tr, {
    keyHelp:'Bu anahtar yalnızca şimdi gösterilir. Güvenli bir parola yöneticisine kaydedin; yeniden gösterilemez.',
    createWarning:'128 bit rastgele anahtar oluşturulur ve yalnızca bir kez gösterilir. Süre hemen başlar.',
    loginText:'İzin verilen Cloudflare hesabınız ve ikinci doğrulamayla devam edin. Ayrı yönetici anahtarı gerekmez.',
    signIn:'Cloudflare ile devam et', tokenHelp:'Yönetici erişimi yalnızca doğrulanmış Cloudflare oturumuyla açılır.',
    sessionHelp:'Oturum Cloudflare Access tarafından yönetilir. Çıkış yapmak Access oturumunu kapatır.',
    unauthorized:'Cloudflare oturumu sona erdi veya bu hesaba izin verilmiyor.',
    publishRelease:'DLL yayınla', releaseVersion:'Sürüm adı',
    releaseHelp:'Doğrulanmış Access oturumuyla DLL yükleyin. En fazla 24 MiB. Dosya özeti otomatik doğrulanır.'
});
Object.assign(messages.en, {
    keyHelp:'This key is shown only once. Save it in a secure password manager; it cannot be recovered.',
    createWarning:'A random 128-bit key is generated and shown only once. The duration starts immediately.',
    loginText:'Continue with your permitted Cloudflare account and second factor. No separate administrator key is needed.',
    signIn:'Continue with Cloudflare', tokenHelp:'Administrator access requires a verified Cloudflare session.',
    sessionHelp:'Cloudflare Access manages your session. Signing out ends the Access session.',
    unauthorized:'Your Cloudflare session expired or this account is not permitted.',
    publishRelease:'Publish DLL', releaseVersion:'Version name',
    releaseHelp:'Upload a DLL using your verified Access session. Maximum 24 MiB. The file hash is checked automatically.'
});
