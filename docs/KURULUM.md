# GHelperMac — Kurulum ve ilk test (yalnızca GU605MY)

Bu paket **sadece** ASUS ROG Zephyrus G16 **GU605MY** (BIOS GU605MY.329) + OpenCore ile çalışan
macOS Sequoia 15.x (x86_64) içindir. Başka bir modelde kullanmayın.

İçerik:
- `ASUSATKD.kext` — ACPI `\_SB.ATKD` (WMNB) köprüsü. Yalnızca beyaz listedeki çağrılara izin verir.
- `GHelperMac.app` — menü çubuğu uygulaması (mod, sıcaklık/fan, fan eğrisi, PL1/PL2).
- `KURULUM.md` (bu dosya), `LICENSE` (GPL-3.0; G-Helper'dan türetilmiştir).

Önemli: Windows'ta G-Helper GPU modu **Ultimate** kalmalı (macOS panel NVIDIA'ya MUX ile bağlıyken çalışıyor).
Bu araç GPU modunu değiştirmez.

---

## 1. Kext'i OPENCORE USB'ye ekleme

1. `ASUSATKD.kext` klasörünü USB'deki `EFI/OC/Kexts/` içine kopyalayın.
2. `EFI/OC/config.plist` dosyasını **kendiniz** açın (ProperTree / OCAT). Önce yedek alın
   (örn. `config.plist.bak-atkd`).
3. `Kernel > Add` dizisinin **sonuna** (itlwm girdisinden sonra) şu girdiyi ekleyin:

```xml
<dict>
	<key>Arch</key>
	<string>x86_64</string>
	<key>BundlePath</key>
	<string>ASUSATKD.kext</string>
	<key>Comment</key>
	<string>GU605MY ATKD fan/PL</string>
	<key>Enabled</key>
	<true/>
	<key>ExecutablePath</key>
	<string>Contents/MacOS/ASUSATKD</string>
	<key>MaxKernel</key>
	<string></string>
	<key>MinKernel</key>
	<string>20.0.0</string>
	<key>PlistPath</key>
	<string>Contents/Info.plist</string>
</dict>
```

4. (İsteğe bağlı, ilk açılış için önerilir) `NVRAM > Add > 7C436110-AB2A-4BBB-A880-FE41995C9F82 > boot-args`
   sonuna boşlukla ` -atkdro` ekleyin. Bu, kext'i **salt-okunur** yapar: hiçbir şey yazılamaz.
   Testten sonra `-atkdro`'yu kaldırın.
5. Kaydedin, USB'den macOS'u başlatın. Windows diskine, boot sırasına ve NVRAM boot girdilerine
   dokunmanız gerekmez.

### Boot-arg'lar
| boot-arg | etkisi |
|---|---|
| `-atkdoff` | kext hiç bağlanmaz (tamamen kapalı) |
| `-atkdro` | bağlanır, okur; **yazmaz** (mod/fan/PL değişmez) |
| `-atkdforce` | DSDT OEM revizyonu farklıysa (BIOS güncellemesi sonrası) yine de yazmaya izin verir — sadece emin olunca |

## 2. Uygulamayı açma

1. `GHelperMac.app`'i `/Applications` içine kopyalayın.
2. İlk açılışta Gatekeeper engeller (imzasız/ad-hoc). Sağ tık > **Aç** veya Terminal:
   `xattr -dr com.apple.quarantine /Applications/GHelperMac.app`
3. Menü çubuğunda `62° D` gibi bir yazı görünür (sıcaklık + mod harfi D/T/S; `+` = özel eğri aktif).

## 3. İlk test — önce SADECE OKUMA

Uygulama ilk açılışta **hiçbir şey yazmaz** (kayıtlı profil yoksa). Siz tıklamadıkça mod/fan/PL değişmez.

1. Kext bağlandı mı:
   ```
   sudo dmesg | grep "ATKD:"
   log show --last boot --style compact --predicate 'eventMessage CONTAINS "ATKD:"'
   ioreg -l -w0 -c ASUSATKD | head -30
   ```
   Beklenen satır (örnek):
   `ATKD: attached to \_SB.ATKD v0.1.0 abi=1 dsdtMatch=1 (oemrev 0x01072009) signature=1 ec=1 modeId=1(0x0001000X) ftbl=0 ro=0 force=0 -> writes ENABLED`
2. Menü panelinde kontrol edin: CPU sıcaklık, CPU/GPU fan RPM, PL1/PL2 (EC), mod (Dengeli/Turbo/Sessiz).
   Windows'taki G-Helper ile aynı mod görünmeli. Fan RPM yük altında artmalı.
3. Ayarlar penceresi > "Tanılama / günlük": `yazma açık · DSDT eşleşti · EC okunuyor · mod-ID var` görmelisiniz.
   Ekran görüntüsü/log'u saklayın.

## 4. İkinci adım — hafif bir fan eğrisi

(`-atkdro` kullandıysanız önce kaldırıp yeniden başlatın.)

1. Ayarlar > Düzenlenen mod = şu anki mod (örn. Dengeli).
2. "Özel fan eğrisi kullan"ı açın. Eğri BIOS fabrika eğrisinden başlar; sadece 1–2 noktayı biraz
   **yükseltin** (daha fazla soğutma = güvenli yön).
3. "Kaydet ve uygula". Günlükte `Fan eğrileri uygulandı` ve `Güç: PL1 … W, PL2 … W (BIOS değerleri)` görünmeli.
   Kext log: `ATKD: DEVS CPU curve T=… P=… -> 0x1`.
   Not: Bu modelde özel eğri etkinleşince EC "manuel" moda geçer; G-Helper gibi uygulama da o anda BIOS'un
   PL değerlerini yeniden yazar.
4. Menüde `+` ve "Özel eğri (FMAE): aktif" görünmeli. Fan sesi eğriye göre değişmeli.
5. Geri almak için: "Varsayılana dön" (aynı mod yeniden seçilir, BIOS kendi eğrisine döner).

## 5. Üçüncü adım — PL1/PL2

1. "Özel güç limiti kullan"ı açın. Kaydırıcılar BIOS'tan okunan değerlerle başlar.
2. Aralıklar (Linux asus-armoury, GU605M): adaptörde PL1 28–90 W, PL2 28–135 W; pilde PL1 25–35 W,
   PL2 38–53 W. PL2 her zaman ≥ PL1. Kext de bu aralık dışını reddeder (PL1 25–90, PL2 28–135).
3. **Yüksek PL = daha yüksek sıcaklık ve fan sesi.** Düşük değerler (ör. 30–40 W) sorunsuz aralıktadır.
4. Bu modelde PL yalnızca özel fan eğrisi etkinken uygulanır; uygulama gerekiyorsa önce eğriyi yazar.
5. fPPT bu BIOS'ta ayarlanamaz (BIOS "desteklenmiyor" döner) — bilerek yok.

## 6. Davranış
- Mod değişince ve uykudan uyanınca BIOS eğrileri sıfırlar; uygulama 5 sn sonra o modun profilini yeniden yazar.
  Ayrıca EC'de özel eğri bayrağı (FMAE) düşerse en fazla 30 sn'de bir yeniden uygular.
- Profiller mod başına: `~/Library/Application Support/GHelperMac/profiles.json`.
- "Girişte başlat" anahtarı macOS giriş öğesi ekler (Sistem Ayarları > Genel > Giriş Öğeleri'nde onay isteyebilir).
- Yazma izni: yalnızca yerel konsol kullanıcısı (veya root). Okuma herkese açık.

## 7. Geri alma (hiçbir şeyi silmeyin)
- Hızlı: `boot-args`'a ` -atkdoff` ekleyin → kext yüklenir ama bağlanmaz.
- Tam: `config.plist`'te ASUSATKD girdisinde `Enabled` = `false` yapın; isterseniz klasörü
  `ASUSATKD.kext.off` olarak yeniden adlandırın (girdiyi kapattıktan sonra).
- Uygulama: Giriş öğesini kapatın, uygulamayı Çöp Sepeti'ne taşıyın. Profil dosyası yukarıdaki yerde kalır.
- macOS'tan çıkıp Windows'a geçtiğinizde G-Helper kendi ayarlarını yeniden uygular.

## 8. Log satırları
| satır | anlamı |
|---|---|
| `ATKD: attached … writes ENABLED` | bağlandı, yazma açık |
| `… writes DISABLED` | salt-okunur: `-atkdro`, DSDT uyuşmazlığı (BIOS değişti?) veya imza eksik |
| `ATKD: DSDT mismatch: ids=… oemrev=…` | yüklenen DSDT beklenen GU605MY.329 dökümü değil → yazma kapalı |
| `ATKD: signature: EC0.XXXX missing` | DSDT'de beklenen EC metodu yok → yazma kapalı |
| `ATKD: EC0 nub not found, using absolute ACPI paths` | bilgi; okumalar mutlak yolla denenir |
| `ATKD: DEVS CPU curve … -> 0x1` | eğri yazıldı (1 = BIOS OK) |
| `ATKD: curve reject: …` | kural ihlali, hiçbir şey yazılmadı |
| `ATKD: PL1 refused: no active custom fan curve` | önce eğri uygulanmalı (uygulama bunu otomatik yapar) |
| `ATKD: … refused: rate limit` | 10 sn'de 24'ten fazla yazma — bekleyin |
| `ATKD: DEVS mode = N -> 0x1` | mod ayarlandı (0 Dengeli, 1 Turbo, 2 Sessiz) |
| `-> 0xfffffffe` | BIOS bu çağrıyı desteklemiyor |

Sorun olursa: `log show --last boot --style compact --predicate 'eventMessage CONTAINS "ATKD:"' > ~/Desktop/atkd.txt`
ve uygulamadaki tanılama metnini paylaşın.
