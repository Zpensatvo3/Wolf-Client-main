#pragma once
#include <string>
#include <string_view>

namespace launcher::ui {
    // Presentation-only translations. Keep transport, validation and launch
    // exceptions intact; known causes must remain distinguishable in the UI.
    inline std::wstring english(const std::wstring& message) {
        struct translation { std::wstring_view original, english; };
        static constexpr translation messages[]{
            {L"Seçilen Minecraft kapandı. Listeyi yenile.",L"The selected Minecraft window closed. Refresh the list."},
            {L"Oturum sona erdi. Tekrar giriş yap.",L"Your session expired. Sign in again."},
            {L"Client dosyası açılamadı.",L"The client file could not be opened."},
            {L"Client dosyası değişmiş. Tekrar giriş yap.",L"The client file changed. Sign in again to verify it."},
            {L"Client zaten bu oyunda açık. İkinci kez yüklenmedi.",L"The client is already running in this game. It was not loaded again."},
            {L"Minecraft'a erişilemiyor. Launcher ve oyunu aynı kullanıcıyla aç.",L"Minecraft is inaccessible. Run both apps as the same Windows user."},
            {L"Güvenli yerel bağlantı oluşturulamadı.",L"Could not create the secure local connection."},
            {L"Bu oyun için başka bir başlatma işlemi var.",L"Another launch is already in progress for this game."},
            {L"Başlatma onayı hazırlanamadı. Biraz bekleyip tekrar dene.",L"Could not prepare launch confirmation. Wait a moment and retry."},
            {L"Yerel bağlantı hazırlanamadı.",L"Could not prepare the local connection."},
            {L"Yerel bağlantı açılamadı.",L"Could not open the local connection."},
            {L"Başlatma izni alınıyor...",L"Requesting launch permission..."},
            {L"Başlatma izni geçersiz.",L"Launch permission is invalid."},
            {L"İşlem iptal edildi.",L"The operation was cancelled."},
            {L"Client Minecraft'a yükleniyor...",L"Loading the client into Minecraft..."},
            {L"Minecraft yükleme alanı hazırlanamadı.",L"Could not prepare Minecraft for loading."},
            {L"Client yolu Minecraft'a iletilemedi.",L"Could not send the client path to Minecraft."},
            {L"Windows yükleyicisi bulunamadı.",L"The Windows loader could not be found."},
            {L"Minecraft Windows yükleyicisi bulunamadı.",L"The Windows loader could not be found in Minecraft."},
            {L"Windows yükleyici adresi doğrulanamadı.",L"The Windows loader address could not be verified."},
            {L"Windows client yüklemesini başlatamadı.",L"Windows could not start loading the client."},
            {L"Yükleme onayı gecikti. Oyunu kapatma; tekrar yüklemeden önce kontrol et.",L"Loading confirmation timed out. Keep the game open and check it before retrying."},
            {L"Client yüklenemedi. Fabric 1.21.1 ve Windows x64 kullan.",L"The client could not load. Check that the release matches your Fabric and Windows x64 setup."},
            {L"Client oturumu doğrulanıyor...",L"Verifying the client session..."},
            {L"Client bağlantısı kurulamadı. Oyundaki hata mesajını kontrol et.",L"The client did not connect. Check the error displayed in Minecraft."},
            {L"Yanlış işlem bağlantısı reddedildi.",L"A connection from the wrong process was rejected."},
            {L"Başlatma izni iletilemedi.",L"Could not deliver launch permission."},
            {L"Başlatma izni eksik iletildi.",L"Launch permission was not fully delivered."},
            {L"Client oturumu onaylanmadı. Oyundaki giriş durumunu kontrol et.",L"The client session was not confirmed. Check the sign-in status in Minecraft."},
            {L"Client hazır. İyi oyunlar.",L"Client is running."},
            {L"Güncelleme imzası doğrulanamadı.",L"The update signature could not be verified."},
            {L"Güncelleme bilgileri geçersiz.",L"The update manifest is invalid."},
            {L"Yerel klasör açılamadı.",L"The local download folder could not be opened."},
            {L"İnternet bağlantısı başlatılamadı.",L"The internet connection could not be initialized."},
            {L"Sunucuya bağlanılamadı.",L"Could not connect to the server."},
            {L"İndirme başlatılamadı.",L"The download could not be started."},
            {L"Güvenli bağlantı kurulamadı.",L"A secure connection could not be established."},
            {L"İndirme bağlantısı kesildi. Tekrar dene.",L"The download connection was interrupted. Please retry."},
            {L"İndirme reddedildi veya sürüm henüz hazır değil. Tekrar giriş yap.",L"The download was denied or this release is not ready. Sign in again."},
            {L"İndirme dosyası oluşturulamadı.",L"The download file could not be created."},
            {L"İndirme tamamlanamadı.",L"The download could not be completed."},
            {L"İndirilen dosyanın boyutu geçersiz.",L"The downloaded file size is invalid."},
            {L"Dosya kaydedilemedi.",L"The downloaded file could not be saved."},
            {L"Dosya eksik indirildi.",L"The download is incomplete."},
            {L"Dosya doğrulanamadı; çalıştırılmadı.",L"File verification failed. The client was not launched."},
            {L"Güncelleme yerleştirilemedi. Client zaten açık olabilir.",L"The update could not be installed. The client may already be running."}
        };
        for (const auto& entry:messages) if (message==entry.original) return std::wstring(entry.english);
        return message.empty() ? L"The operation could not be completed. Please retry." : message;
    }
}
