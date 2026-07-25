# Proje Devir Notları — Citroën C5 (MK1) CAN Teşhis Arayüzü

> Bu dosya, projeyi başka bir yapay zeka modeline/devreye devretmek için hazırlanmıştır.
> Projenin ne olduğu, hangi aşamada olduğu, alınan kritik kararlar ve bir sonraki adımlar burada.

---

## 0f. GÜNCELLEME — 2026-07-26 (Sniffer yeniden tasarım, ikinci MCP2515 kaldırıldı, CAN ID düzeltmeleri)

### Mimaride köklü değişiklik: tek MCP2515 yeterli

- **Single MCP2515 confirmed.** SPI0 GP2-6. Runtime baud rate switching: `gsniff rate hs/ls`
  calls `Mcp2515::setBaudRate()` which enters config mode, writes new CNF registers, re-applies
  TX priority + RX filters, returns to listen-only mode. `CanManager` simplified — no more
  `DualCanPins`, `ls_`, `ls_ready_`. Second bus drain loop removed from `main.cpp`.
  `hwtest` reduced to single-chip. Docs section 6 rewritten.
- **`gsniff rate` firmware handler: DONE.** MCP2515 baud rate'ini runtime'da degistirip
  listen-only moda geri donuyor. HS (500k) ve LS (125k) icin test edildi.
- **Sniffer artık raw CAN monitor.** ECU fonksiyon tab'larının içinden çıkarıldı, üst bar'da
  "Sniffer" butonu eklendi (Global Test'in solunda). Tıklayınca sidebar/ECU bar/funcTabs gizlenir,
  sniff pane full-screen açılır. "← Back to diagnostics" ile ECU görünümüne dönülür.
  ([dashboard.html](dashboard/dashboard.html#L33), [dashboard.js](dashboard/dashboard.js#L1281))
- **HS (500k) / LS (125k) seçici.** Sniff içinde iki alt-sekme; tıklayınca MCP2515 baud rate'ini
  degistiren `gsniff rate hs/ls` komutu gönderilir (firmware handler eklendi — DONE)
- **Guided sniff UI tamamen kaldırıldı.** Senaryo/Count/Hold/Sweep/Watch/Save/Load butonları ve
  step banner, learned/candidates tabloları dashboard'dan silindi. Backend `gsniff` shell komutları
  firmware'de kalır (ileride tekrar kullanılabilir).
- **Önemli: kullanıcı fiziksel olarak tek MCP2515 kablosunu hangi bus'a takarsa o bus'ı dinler.**
  OBD2 pin 3/8'den CAN-HS; CAN-LS için BSI konnektöründen fiziksel tap gerekir.

### CAN broadcast decoder düzeltmeleri (JustNoLimit/citroen-can referansı)

Kaynak: https://github.com/JustNoLimit/citroen-can — C5 Mk1 FL için verified CAN ID veritabanı.
- **0x0F6 ext_temp byte offset düzeltildi** (d[5]→d[6]). Eklendi: coolant (d[1]-40), odometer (d[2:4])
- **0x208 tamamen yanlıştı**: gaz pedalı % × 0.392 + tork (önceden RPM+throttle+brake sanıyordu)
- **0x488 oil temp byte düzeltildi** (d[5]→d[1])
- **30+ yeni CAN ID decoderı eklendi.** HS: 0x120, 0x128, 0x168(detaylı), 0x305, 0x348, 0x44D,
  0x468, 0x108, 0x34D, 0x38D, 0x072, 0x0A8, 0x217. LS: 0x1E1, 0x1A1, 0x221, 0x228, 0x336/3B6/2B6,
  0x3E5, 0x2A5, 0x225, 0x125, 0x1A8, 0x0D6, 0x220, 0x227, 0x5E5, 0x1A5, 0x0A4, 0x165.
  ([main.cpp](src/main.cpp#L17))
- **Dokümantasyon güncellendi.** `docs/psa_can_reference.md` bölüm 2: HS→17 ID, LS→34 ID, Verification
  seviyeleri (TESTED/COMMUNITY) eklendi.

### Kalan işler

- ECU adres tablosu doğrulandı, değişmedi.
- Hiçbir değişiklik gerçek araçta denenmedi (ISO-TP akış kontrolü, MCP2515 öncelik, flash sırası).
- **Eksik ECU PIN'leri (CLIM, HDC, SPNEU, AUTORADIO, ADC, MDP_*, PROJECTEURS, AIDE_STAT) önemli değil.** 
  Bu ECU'ların çoğu BSI arkasındaki LIN/periferik modüller; SecurityAccess BSI üzerinden yapılır,
  ayrı PIN gerekmez. Kalanlar için kamuya açık kaynak yok, `pin <hex>` ile elle girilir.
  Üç dilde (FR/EN/RU) arandı, bulunamadı — tekrar aramaya gerek yok.

---

## 0e. GÜNCELLEME — 2026-07-25 (denetim + düzeltme turu: ISO-TP akış kontrolü, stack, flash güvenliği)

Kod tabanı 10 alt sistemde ajan filosuyla denetlendi, her bulgu düşmanca bir doğrulama turundan
geçirildi ve onaylananlar düzeltildi. Ayrıca dış kaynaklar (ludwig-v, prototux/PSA-RE,
Melnik-Alex/PSA_CAN) tarandı. **Bu bölüm aşağıdaki §6 ve §7'nin bir kısmını geçersiz kılar.**

### Kritik — cihazı kilitleyen / belleği bozan

- **`main()` 2 KB'lık stack'e 74.752 bayt yerel değişken koyuyordu.** `DiagShell` (74.904 bayt) ve
  `CanManager` `main()` içinde yereldi; `__StackTop=0x20082000`, `__StackBottom=0x20081800`, yani
  core-0 stack'i sadece 2 KB. Derleyici `main`'e `sub.w sp, sp, #74752` üretiyordu → SP, tanımlı
  stack tabanının **72 KB altına** iniyordu. O bölge statik olarak kullanılmadığı için tesadüfen
  patlamıyordu. Nesneler `.bss`'e taşındı ([main.cpp](src/main.cpp)); `main()` çerçevesi artık
  **32 bayt**. `SRecord::data` gerçekçi boyuta indirildiği için `DiagShell` de 74.904 → ~32 KB oldu.
  bss 83.752 → 115.724 (520 KB'nin içinde rahat).
- **Firmware hiç derlenmiyordu.** [CMakeLists.txt](CMakeLists.txt) `PICO_BOARD` pinlemiyordu →
  dokümante edilen `cmake -B build` varsayılan `pico` (RP2040, cyw43 yok) ile configure edip
  `lwip/tcp.h` bulunamadığından ölüyordu. `set(PICO_BOARD pico2_w CACHE ...)` eklendi (project()
  öncesinde olmak zorunda).
- **Flash S-record payload'ı 16 baytlık `Req::buf`'a `memcpy` ediliyordu.** `SRecord::data` 256
  bayt, `data_len` sınırsız (252'ye kadar) — sıradan bir 32 baytlık S1 kaydı çağıranın stack
  çerçevesini eziyordu, üstelik ECU zaten silinmiş ve bootloader oturumu açıkken.
  `Req::buf` `kMaxFlashBlock+2`'ye genişletildi, `SRecord::data` aynı boyuta indirildi, parser
  fazlasını **reddediyor** (kırpmıyor — kırpmak ECU imajında delik bırakırdı), `memcpy` ayrıca
  clamp'lendi. ([psa_protocol.hpp](include/psa/psa_protocol.hpp),
  [flash_engine.hpp](include/psa/flash_engine.hpp), [flash_engine.cpp](src/flash_engine.cpp))
- **`hwtest` `ready()` guard'ını deliyordu.** `can_->hs()`/`ls()` ham referans döndürüyordu; çipi
  takılı olmayan bus'ta `spi_write_blocking` sonsuza kadar bekleyip ana döngüyü kilitliyordu —
  yani §0c'de "ilk seri bağlantıda çalıştır" denen komut, teşhis etmesi gereken donanımda cihazı
  öldürüyordu. Ham erişimciler kaldırıldı; `CanManager::bus(Bus)` hazır olmayan bus için
  **nullptr** döndürüyor, guard artık unutulamaz. ([can_manager.hpp](include/psa/can_manager.hpp))
- **Cevap zaman aşımı prosedürü temizlemiyordu.** Timeout bloğu `state_`'i `Idle`'a alıyor, altındaki
  prosedür kontrolü `WaitingResponse` göremediği için `abortProcedure()` hiç çalışmıyordu → `proc_`
  set kalıyor ve sonradan gelen alakasız bir yanıt bayat bir yazma dizisini devam ettirebiliyordu.
  Tek bloğa birleştirildi; timeout artık `config_readall_active_`, `flash_active_`, `pdi_active_`,
  `live_polling_active_` ve `proc_`'un hepsini söküyor.
- **TesterPresent bekleyen isteğin üstüne gidiyordu.** Keep-alive `WaitingResponse` sırasında da
  gönderiliyordu; ona gelen bir negatif yanıt, bekleyen isteğin reddi sanılıp devam eden flash'ı
  veya prosedürü iptal ediyordu. İstek uçuştayken keep-alive susturuldu.
- **Watchdog hiç yoktu.** Wi-Fi ayağa kalktıktan sonra 8 sn'lik watchdog arm ediliyor
  (5 sn'lik ISO-TP timeout'unun rahat üstünde), döngüde `watchdog_update()`.

### ISO-TP — çok-frame'li her şey kırıktı

- **Gönderici tarafı akış kontrolünü tamamen yok sayıyordu.** `encode()` First Frame + bütün
  Consecutive Frame'leri üretiyor, `sendPdu()` hepsini arka arkaya bus'a basıyordu. Gerçek ECU
  bunları düşürür → **telekodlama yazmaları ve flash yolu hiç çalışmıyordu**. FC.OVERFLOW (0x32),
  yani "kabul etmiyorum" cevabı da yutuluyordu; ECU açıkça reddederken yazmaya devam ediyorduk.
  Artık gerçek bir gönderici durum makinesi var ([isotp.hpp](include/psa/isotp.hpp),
  [isotp.cpp](src/isotp.cpp)): `beginSend()` FF'i yollayıp bekler, `feed()` FC'yi çözer
  (FS=CTS/WAIT/OVERFLOW, BS, STmin — `0xF1-0xF9` mikrosaniye kodlaması dahil), `nextTxFrame()`
  blok boyutu ve STmin'e uyarak CF'leri `poll()`'dan pompalar, N_Bs (1 sn) dolunca iptal eder.
- **Alıcı tarafı BS=0/STmin=0 ilan ediyordu** (sınırsız ak) ama ana döngü bus başına turda tek
  frame çekiyordu. Artık sınırlı blok (`kRxBlockSize=8`) + `kRxStMinMs=1`, ve ana döngü her turda
  iki bus'ı da (sınırlı biçimde, `kMaxRxPerPass=8`) boşaltıyor.
- `sendPdu` `McpError`'ı atıyordu; 4. frame'den itibaren sessizce düşüyordu. Artık `sendFrame()`
  sınırlı retry yapıyor ve başarısızlığı **kullanıcıya bildiriyor**.

### MCP2515

- **Üç TX buffer'ı da eşit öncelikteydi.** Datasheet eşitlikte **yüksek numaralı** buffer'ı önce
  yolluyor → çok-frame'li her isteğin CF'leri ters sırada gidiyordu. TXB0>TXB1>TXB2 azalan öncelik
  `init()`'te set edildi.
- TXREQ timeout'u, bus-off tespiti ve hata sayacı yönetimi yoktu: kimse ACK vermezse (kontak
  kapalı, konnektör takılı değil) TXREQ hiç temizlenmiyor, üç buffer da kilitleniyor ve o bus
  **güç kesilene kadar** bir daha veri gönderemiyordu. `errorFlags()`/`busOff()`/`recoverBus()`
  eklendi; `send()` bütün buffer'lar doluyken TXBO/TXEP görürse kuyruğu boşaltıyor.
- 29-bit genişletilmiş ID'ler `uint16_t`'a kırpılıyordu (ama `ext=true` raporlanıyordu).
  `CanFrame::id` `uint32_t` oldu.
- `hwtest`'in SPI varlık testi **CNF1**'e yazıyordu; CNF register'ları yalnızca configuration
  mode'da yazılabilir, o an çipler Normal mode'daydı → **sağlam donanımda FAIL** veriyordu.
  Artık her modda yazılabilen `CANINTE` ile probe ediliyor ve eski değeri geri yazılıyor.

### Flash yolu

- **Programlama oturumu hiç açılmıyordu**; erase/download doğrudan gönderiliyordu. Artık
  `flash begin` önce programlama oturumu açıyor (`startProgrammingSession()`), onay gelince erase.
- **Veri olmayan S-record'lar (S0/S5/S7/S8/S9) firmware payload'ı olarak yollanıyordu** — S0'ın
  adresi (0) flash hedefi oluyordu. Artık yalnızca S1/S2/S3 stage ediliyor.
- **RequestDownload adres 0 ve sabit 64 KB bildiriyordu** (boş bir `dummy` kayıttan üretiliyordu).
  Artık stage edilmiş imajın gerçek adres/boyutunu bildiriyor, ve ECU'nun yanıtındaki
  `maxNumberOfBlockLength` okunup TransferData blok boyutu ona göre kısıtlanıyor.
- **Akış sırası düzeltildi:** erase/download el sıkışması milisaniyeler içinde bitip stage listesini
  yürüdüğü için, `flash begin` listeyi sıfırladığında TransferData'ya **hep boş** varılıyordu.
  Artık önce `flash <S-record>` satırlarıyla imaj stage ediliyor, sonra `flash begin`; kayıt yoksa
  `begin` reddediyor. `flash cancel` listeyi temizliyor.

### Veri / arayüz

- **0x3A7 bakım çerçevesi yanlış baytları okuyordu** (km ve gün ikisi de yanlış). İki bağımsız
  kaynak (ludwig-v comfort adapter, Melnik-Alex/PSA_CAN) km'yi `data[3..4]`, günü `data[5..6]`
  diyor; düzeltildi.
- **0x1D0 klima çözümlendi** (fan, üfleme yönü, demist/devirdaim, sol/sağ sıcaklık, MONO tespiti) —
  kaynak: ludwig-v/arduino-psa-comfort-can-adapter. Bu, custom head-unit hedefinin ana çerçevesiydi.
- **Kaynağı doğrulanamayan 4 decoder** (0x208, 0x488, 0x161, 0x0E8) çıktısında artık `(unverified)`
  etiketi taşıyor — üç bağımsız kaynakta da bu ID'ler yok, ölçek/offset tahmin.
- `uds::NRC::InvalidKey` 0x13'tü; ISO 14229'da doğrusu **0x35** (0x13 = incorrectMessageLength).
- Çözülemeyen bir parametrenin `[LIVE]` ham çıktısı iki nokta içeriyordu; panonun ölçüm regex'i
  (`/^\[LIVE\] (.+): ([-0-9.]+)/`) ilk hex baytı **ölçülen değer sanıp grafiğe çiziyordu**.
  Ayraç değiştirildi, pano tarafı değişmedi (asset yeniden üretmek gerekmedi).

### Ağ katmanı ([wifi_server.cpp](src/wifi_server.cpp))

HTTP isteği tek bir `recv()` çağrısından parse ediliyordu → TCP segmentlerine bölünen istek
(uzun bir `/api/cmd?val=...` için normal) yarım yorumlanıyordu; artık bağlantı başına `CRLFCRLF`
görülene kadar biriktiriliyor (sabit havuz, `kMaxConns=4`, malloc yok), sınır aşılırsa 400.
`tcp_recved()` hiç çağrılmıyordu → alma penceresi kapanıp bağlantı kalıcı olarak tıkanıyordu.
`urlDecode()` sonda tek `%` görünce tamponun dışını okuyordu. `tcp_write()`'ın `ERR_MEM`'i
kontrol edilmiyordu → ~19 KB'lık `dashboard.js` sessizce kırpılabiliyordu; artık `tcp_sent()`
callback'inden devam ediyor. lwIP callback bağlamındaki `printf`'ler kaldırıldı, yerine düşen SSE
pcb'si gerçekten kapatılıyor, SSE dışı yanıtlar boşalınca kapatılıyor.
**Pano sözleşmesi (§2, §5) bit düzeyinde korundu** — hiçbir route, header veya SSE satır formatı
değişmedi.

### Depo hijyeni

`stdout.log`/`stderr.log` git'te takip ediliyordu → kaldırıldı, `*.log` ignore edildi.
`include/psa/actuator_catalog.hpp` takip edilmiyordu ama `diag_shell.cpp` onu include ediyordu
(temiz bir klonda derleme kırılırdı) → eklendi.

### Doğrulama

- Host self-check: **27 kontrol, hepsi geçiyor.** Yeni testler: ISO-TP gönderici akış kontrolü
  (FC.CTS/WAIT/OVERFLOW + BS + STmin + N_Bs timeout), alıcı FC parametreleri, S-record sınırları ve
  kayıt-tipi sınıflandırması, flash extent/oturum sırası, reddedilen iletimin yutulmaması.
- `cmake -B build_host -DHOST_TEST=ON` yolu da geçiyor.
- Firmware: `cmake -B build && cmake --build build` **bayraksız** temiz derleniyor
  (text 467.012 / 4 MB flash, bss 115.724 / 520 KB SRAM), `.uf2` üretiliyor.
- `node --check dashboard/dashboard.js` geçiyor; `dashboard_assets.h` senkron (pano dosyaları
  değişmedi).

### YAPILMAYANLAR / açık kalanlar

- **Hiçbiri gerçek araçta denenmedi.** Bu ortamda araç/CAN bus yok; `hwtest` ve `gsniff` ile aynı
  durumdayız: mantık host testiyle doğrulandı, fiziksel doğrulama kullanıcıya kalıyor.
  Özellikle ISO-TP akış kontrolü, MCP2515 TX önceliği ve flash sırası araçta teyit edilmeli.
- **PSA BSI aktüatör testi RoutineControl ID'leri hâlâ bilinmiyor.** ludwig-v, prototux/PSA-RE ve
  Melnik-Alex/PSA_CAN'de kamuya açık kaynak yok. [actuator_catalog.hpp](include/psa/actuator_catalog.hpp)
  bilerek sadece **isim** taşıyor, ID taşımıyor — yanlış ID'yi gerçek araçta ateşlemek güvenli değil.
- **[ecu_params.hpp](include/psa/ecu_params.hpp)'deki `0x3E01`-`0x3E05` aktüatör ID'lerinin kaynağı
  yok** (marş rölesi, yakıt pompası, fan, kızdırma bujisi). Araçta doğrulanana kadar şüpheli
  sayılmalı — bunlar gerçekten fiziksel aktüatör tetikliyor.
- `ecu_params.hpp`'deki **BSM bloğu adreslenebilir değil** (BSM, BSI'nın adresinde cevap veriyor;
  §0'da kEcuTable'dan çıkarılmıştı). Referans veri olarak duruyor, hiçbir yerden erişilmiyor.
  BMF'ye birleştirmeden önce araçta doğrulanmalı — o adresin arkasındaki yanlış ECU'ya yazmak
  geri alınabilir bir hata değil.
- CAN2004/C5 Mk1 FL için **kilometre sayacı ve far/sinyal durumu broadcast çerçevesi bulunamadı**
  hiçbir kamuya açık kaynakta. Canlı capture gerekiyor.
- **Ölçüm tek parametre** mimari kısıtı (§6) değişmedi.
- (Yok) `tests/test_psa.cpp` başındaki derleme komutu da bu turda güncellendi.

---

## 0d. GÜNCELLEME — 2026-07-20 (kılavuzlu/komutlu CAN sniffing — sinyal keşif aracı)

- **Amaç:** RD4 yerine custom head-unit/dashboard yapabilmek için önce bilinmeyen broadcast
  CAN sinyallerinin (özellikle klima 0x1D0) bayt yerleşimini çıkarmak. Düz sniff tek başına
  yorumlanamaz (LS bus'ta ~40 ID sürekli değişir); çözüm: bilinen bir fiziksel eylemi (N kez
  butona bas / değeri sabit tut / yavaşça değiştir) sayılabilir bir imzaya çevirip CAN
  baytlarıyla eşleştirmek.
- **Yeni modül:** [can_sniffer.hpp](include/psa/can_sniffer.hpp) / [can_sniffer.cpp](src/can_sniffer.cpp)
  — `CanSniffer` sınıfı, donanımsız (HOST_TEST altında test edilir). ID başına (bus+id ile
  anahtarlı, `kMaxSlots=64`) bayt bazında değişim sayacı/min/max/yön tutan bir tablo (~4 KB
  RAM — RP2350'nin 520 KB SRAM'inde önemsiz).
  - **3 yakalama modu:** `Count` (N kez değişim bekleniyor — butonlar), `Hold` (yeni bir
    değerde sabit kalıyor — RPM/setpoint tutma), `Sweep` (tek yönlü monoton değişim — yavaş
    devir yükseltme). Skorlama: `matchScore()`, düşük=daha iyi eşleşme.
  - **Baseline maskeleme:** `beginBaseline()` öncesi kendiliğinden değişen (ID,bayt) çiftlerini
    `noisy_mask` ile işaretler; sonraki tüm modlarda bu baytlar aday listesine hiç girmez.
  - **Senaryo yürütücü (asıl kullanım şekli):** `beginRun()`/`nextStep()` — hazır bir adım
    listesini (`Step{prompt,mode,expected,label}`) sırayla yürütür, her adımda en iyi adayı
    otomatik etiketleyip `learned_[]`'e yazar. İlk senaryo: `kClimateScenario` (10 adım: şoför/
    yolcu sıcaklık ±, fan ±, auto, A/C, hava yönü, devirdaim).
  - **Flash kalıcılığı (isteğe bağlı, `#ifndef HOST_TEST`):** `save()/load()` sadece damıtılmış
    `learned_[]` haritasını (birkaç KB) son flash sektörüne (`PICO_FLASH_SIZE_BYTES -
    FLASH_SECTOR_SIZE`) yazar/okur, `flash_safe_execute()` ile. Bu proje core1 hiç başlatmıyor
    → `PICO_FLASH_ASSUME_CORE1_SAFE=1` derleme tanımı eklendi ([CMakeLists.txt](CMakeLists.txt)),
    `hardware_flash`/`pico_flash` linklendi. Ham frame kaydı YOK, sadece sonuç.
- **Komut:** `gsniff <alt-komut>` — `run <senaryo> | next | base [sn] | count N | hold | sweep |
  stop | report | watch <hs|ls> <hexid> | save | load | clear | status`.
  [diag_shell.hpp](include/psa/diag_shell.hpp) / [diag_shell.cpp](src/diag_shell.cpp)'de
  `cmdGuidedSniff()`; dispatch `processLine()`'a eklendi, `poll()` içinde `sniffer_.tick()`
  baseline penceresini otomatik kapatıyor.
- **Frame yönlendirme:** [main.cpp](src/main.cpp) RX drenajında, `feedDiagFrame` tüketmediği
  frame'ler için: `gsniffActive()` (capturing VEYA watch açık) ise sniffer'a, değilse eskisi
  gibi `decode_sniffed()`'e gider — pasif `sniff on/off` ile çakışmıyor.
- **Web arayüzü:** Yeni **Sniffer** sekmesi ([dashboard.html](dashboard/dashboard.html)/
  [dashboard.js](dashboard/dashboard.js)) — ECU connect/unlock GEREKTİRMEZ (ham bus aracı,
  `setConnected` disable listesinde değil). Senaryo seçici + "Sonraki adım" büyük buton +
  adım-metni banner'ı; altında ad-hoc mod seçici (Count/Hold/Sweep) + Baseline/Start/Stop;
  öğrenilen-harita ve aday tabloları `[GSNIFF] ...` SSE satırlarından `handleLine()`'da
  regex ile dolduruluyor (bkz. dosyanın "guided sniffer" bölümü). **Değişince
  `python3 scripts/generate_assets.py` çalıştırılmalı** (yapıldı, `dashboard_assets.h`
  güncel).
- **Yan düzeltme:** `CMakeLists.txt`'nin `HOST_TEST` dalı `target_compile_definitions(test_psa
  PRIVATE HOST_TEST)` içermiyordu — `cmake -B build_host -DHOST_TEST=ON` dokümante edilen
  yol hiç derlenmiyordu (sadece §6'daki manuel `g++ -DHOST_TEST ...` komutu çalışıyordu).
  Tek satır eklenip düzeltildi; artık ikisi de çalışıyor.
- **Doğrulama:** `cmake -B build_host -DHOST_TEST=ON && cmake --build build_host &&
  ./build_host/test_psa` → 4 yeni `can_sniffer` testi dahil hepsi geçiyor (Count/Hold/Sweep
  + 10 adımlık senaryo FSM'i uçtan uca). Firmware de gerçek Pico SDK ile (`/home/umut/pico/pico-sdk`)
  temiz derlendi (`.uf2` üretildi, text≈452 KB / 4 MB flash, bss≈84 KB / 520 KB SRAM).
  **Gerçek donanımda henüz denenmedi** (bu ortamda araç/CAN bus yok) — `hwtest` ile aynı
  durumda: mantık doğrulandı, fiziksel doğrulama kullanıcıya kalıyor. Web tarafı statik bir
  önizleme sunucusuyla görsel olarak kontrol edildi (sekme render oluyor, buton→komut kablolaması
  çalışıyor, konsol hatasız); gerçek SSE akışı cihaz olmadan test edilemedi.

---

## 0c. GÜNCELLEME — 2026-07-20 (donanım self-test + USB CDC/DTR fix)

- **Ölü bus izolasyonu (kritik):** Bir MCP2515 fiziksel olarak takılı değilse, Wi-Fi/cyw43
  devreye girdikten sonra o bus'un SPI periferiği kilitlenip (`spi_write_blocking` sonsuza
  kadar bekleyip) tüm ana döngüyü durdurabiliyordu. `CanManager` artık `init()` sırasında
  her bus'u **bağımsız** açıyor ve hangi bus'ların gerçekten cevap verdiğini
  (`hs_ready_`/`ls_ready_`) tutuyor; `ready()` false dönen bus'a `hasRx`/`read`/`send` bir
  daha dokunmuyor. Bkz. [can_manager.hpp](include/psa/can_manager.hpp),
  [can_manager.cpp](src/can_manager.cpp).
- **Yeni `hwtest` shell komutu** ([diag_shell.cpp](src/diag_shell.cpp)): SPI kablolamasını
  (CNF1'e bilinen bir bayt yazıp geri okuyarak — sabit `!= 0xFF` kontrolü floating hat için
  güvenilmez çıktı) ve CAN loopback'i (`Mcp2515::setLoopbackMode()`, yeni) her bus için ayrı
  ayrı doğruluyor. **Gerçek donanımda henüz denenmedi** — host test'te `#ifdef HOST_TEST` ile
  devre dışı, sadece derleme/mantık doğrulandı.
- **USB CDC / shell "ölü" görünme sorunu düzeltildi:** Pico SDK'nın USB stdio'su varsayılan
  olarak DTR sinyaline gate'li; Termix gibi GUI terminaller DTR kaldırmıyor. Çözüm:
  `PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1` derleme tanımı ([CMakeLists.txt](CMakeLists.txt))
  + ana döngüde her turda `tud_task()` çağrısı ([main.cpp](src/main.cpp)) — SDK'nın arkaplan
  görevi cyw43 threadsafe_background altında aç kalabiliyordu.
- `test_psa` artık git'te takip edilmiyor (derlenmiş binary, `.gitignore`'a eklendi);
  `hardware/` (FreeCAD kutu tasarımı + STEP parçaları) repoya eklendi.

---

## 0. GÜNCELLEME — 2026-07-18 (tersine mühendislik + düzeltme oturumu)

Kod derinlemesine incelendi, ludwig-v kaynaklarına karşı doğrulandı ve arayüz sıfırdan
yeniden yazıldı. Bu bölüm aşağıdaki (eski) notların bir kısmını **geçersiz kılar**.

**Kritik düzeltmeler (hepsi host self-check ile doğrulandı):**
- **SSE log yolu tamir edildi (ana neden!):** Kabuk 330 `printf` kullanıyor ama sadece
  `diagLog` (1 çağrı) SSE'ye gidiyordu → panoya araçtan **hiçbir veri akmıyordu**. Artık
  `WifiServer::initStdioCapture()` tüm stdout'u satır satır yakalayıp SSE'ye yolluyor
  (`src/wifi_server.cpp` içinde pico stdio driver). `setLogSink` kaldırıldı.
- **ISO-TP bellek taşması:** ≤7 baytlık sahte First Frame `size_t` underflow + `memcpy`
  taşması yapıyordu. Artık bozuk SF/FF reddediliyor (`src/isotp.cpp`).
- **ECU adres tablosu:** ludwig-v ECU_LIST.md'ye göre 5 yanlış adres düzeltildi
  (ALARME→75C:65C, MDP_CONDUCT→756:656, MDP_PASSAG→755:655, PROJECTEURS→6B7:697), BSM
  atıldı (BSI ile aynı adres). **Artık 24 değil 23 ECU.** ECRAN_C/AIDE_STAT doğrulanamadı.
- **Güvenlik PIN'leri uyduruktu:** Eski `ecu_keys.hpp` değerleri gerçek değildi (0xD91C
  aslında NAC/TELEMAT anahtarı). Artık sadece kaynak-doğrulanmış aile varsayılanları var
  (BMF/BSI=B2B2, INJ=475A, TELEMAT=D91C); gerisi 0x0000. Gerçek anahtar ECU-modeline özgü →
  yeni **`pin <hex>`** komutu ile elle girilir (bilinmeyen PIN'de unlock artık çöp anahtar
  göndermeyip uyarı veriyor).
- Diğer: UDS DTC offset (pos 2→3), KWP write yanıtı, traceability trailing `00 00`,
  flash `end` finalize (transfer-exit→checksum), 0x78 sonsuz-döngü sınırı, prosedür NRC
  iptali, tek doğrulanmış hex parser (`parseHexU16`), odometer decoder, `/api/data`
  gerçek durum.

**Arayüz tamamen yeniden yazıldı (Lexia 3 / Diagbox klonu):**
- Eski `dashboard/*` (v1 + v2) **silindi**. Yeni: `dashboard.html/css/js` (modern koyu tema).
- Düzen Lexia'yı birebir izler: üst **Superviseur** çubuğu (araç durumu göstergeleri +
  Test Global) → sol **calculateur ağacı** (durum noktaları + DTC rozeti) → ECU seçince
  **fonksiyon sekmeleri** (Identification / Défauts / Mesures / Actionneurs / Télécodage /
  Téléchargement) → alt **console** (SSE + serbest komut).
- Backend sözleşmesi (aşağıdaki §2, §5) **değişmedi**; yeni JS aynı komutları/aynı SSE
  satırlarını kullanır. Tarayıcıda uçtan uca doğrulandı (scan→ağaç, dtc, live, config).
- Charset: firmware artık `text/html; charset=utf-8` yolluyor + `<meta charset>` var.

**Not:** `ecu_params.hpp` firmware'e derlenmez (yalnızca `gen_ecu_data.py` panoyu üretir).
Onun decoder uyumsuzlukları çalışma zamanını etkilemez; runtime decoder'lar `live_data.hpp`'de.

---

## 1. Proje Nedir?

Açık kaynak bir **Lexia 3 (PSA/Citroën) muadili** teşhis cihazı. Donanım: **Raspberry Pi Pico 2 W (RP2350)** + 2× **MCP2515** CAN controller, KWP2000 / UDS / ISO-TP üzerinden PSA ECU'larıyla konuşur, **Wi-Fi AP modunda** (`192.168.4.1`) bir web arayüzü sunar.

Hedef: Lexia 3'ün tüm menü işlevlerini (global test, DTC okuma/silme, parametre okuma/yazma, aktüatör testleri, ölçüm, programlama/yazılım, PDI raporu) bu cihazda sunmak.

---

## 2. Mimari ve Kısıtlar (ÖNEMLİ)

- **Dil:** C++17 (RP2350 için), Pico SDK, lwIP raw TCP HTTP server (Wi-Fi AP modu, FreeRTOS **yok**).
- **UI taşıma:** Dashboard dosyaları (`dashboard.html/css/js`) **gzip** ile sıkıştırılıp C++ header'a gömülür → `include/psa/dashboard_assets.h`. Cihaz bunları HTTP ile servis eder.
- **KATI KISITLAR:** Harici JS/CSS/CDN yok, Web Serial yok, harici font yok. Sadece sistem fontları ve saf JS/CSS. Gömülü dosyalar elle düzenlenmez — `scripts/generate_assets.py` ile üretilir.
- **Dashboard ↔ cihaz haberleşme sözleşmesi (DEĞİŞTİRME):**
  - Komutlar: `GET /api/cmd?val=<cmd>`  (cmd URL-decode edilir, boşluk `+` ile gelir)
  - Canlı log akışı: `EventSource('/api/stream')` (SSE)
  - JSON veri: `GET /api/data?type=vehicle` → `{vin,model,year}`; `GET /api/data?type=status` → `{connected}`
- **Backend komut ismi çözümü:** `connect <arg>` / `dtc` / `unlock` ECU'yu **family adıyla** çözüyor (örn. `INJ`, `BMF`, `ABRASR`). Dashboard ECU id'leri bu yüzden family adlarıdır (bkz. Bölüm 5).

---

## 3. Dizin Yapısı

```
dashboard/dashboard.html, dashboard.css, dashboard.js  <- EDITLENEN arayüz (gömülü)
docs/psa_can_reference.md                              <- PSA CAN protokol referansı (ana teknik referans)
docs/lexia3_menu_reference.md                          <- Lexia 3 menü/parametre referansı (video analizinden)
docs/ECU_KEYS.md                                       <- ludwig-v gerçek tedarikçi güvenlik PIN veritabanı
include/psa/
  psa_protocol.hpp   <- kEcuTable (24 ECU), readZone/writeZoneHeader, findEcuIdentification
  ecu_params.hpp     <- 24 ECU için config/meas/actuator veritabanı (OTORİTE kaynak)
  ecu_zones.hpp      <- BSI config zone parametreleri (141 param)
  ecu_keys.hpp       <- 24 ECU güvenlik PIN'i
  live_data.hpp      <- ölçüm decode fonksiyonları, kKwpParams/kUdsParams/kLiveDataCategories
  diag_shell.hpp/.cpp<- komut işleyici (processLine dispatch)
  wifi_server.hpp/.cpp<- HTTP/SSE sunucu
  flash_engine.hpp/.cpp<- yazılım yükleme state machine
  can_manager.hpp, mcp2515.hpp, isotp.hpp, dashboard_assets.h (generated)
src/                 <- .cpp uygulamaları (diag_shell.cpp en kritik)
scripts/generate_assets.py  <- dashboard.* dosyalarını gzip→C header'a çevirir
tests/test_psa.cpp   <- host test
```

---

## 4. Mevcut Durum (tamamlananlar)

### C++ (temiz derleniyor — bkz. Bölüm 7 uyarı)
- 24 ECU `kEcuTable`'de tanımlı; hepsi için config/meas/actuator veritabanı (`ecu_params.hpp`).
- `readZone`/`writeZoneHeader`: zone_id > 0xFF ise UDS `22/2E` çerçevesi (2-byte DID), değilse KWP.
- KWP_IS yazma servis byte'ı `0x34`, pozitif yanıt `0x74`.
- `0xC2` session-kapatma yanlış yorumlanması düzeldi (artık `0xC1` ile ayrı).
- Cruise control maskesi `0x08`→`0x0C`.
- 24 ECU güvenlik PIN'i dolduruldu (`ecu_keys.hpp`).
- Timeout handler `config_readall_active_`'i temizleyip `Idle`'a dönüyor.
- `cmdLive`/`cmdMeas` `WaitingResponse` state'inde reddediyor.
- PDI komutu + rapor (`printScanResults`), firmware flash begin/end/status/cancel + S-record.

### Dashboard (YENİDEN TASARLANDI — son iş)
- `dashboard.html/css/js` sıfırdan modern koyu tema ile yazıldı (sidebar nav, 7 sekme).
- **Kaybolan veri kurtarıldı:** `ECU_CONFIG_PARAMS`/`ECU_MEAS_PARAMS`/`ACTUATOR_TESTS` (24 ECU) `ecu_params.hpp`'dan parse edilerek yeniden üretildi. Üretici: `/tmp/gen_ecu.py` (geçici, yeniden çalıştırılabilir).
- Backend sözleşme uyumsuzlukları giderildi (Bölüm 5).
- `dashboard_assets.h` yeniden üretildi ve gzip doğrulandı.

---

## 5. Kritik Kararlar ve Düzeltmeler (SON OTURUMDA)

Bu düzeltmeler **görsel değil, fonksiyonel** ve doğru çalışması zorunludur:

1. **ECU id = backend family adı.** Eski dashboard `bmf`/`bsi`/`abs` gibi id'ler kullanıyordu; backend `connect` bunları family adıyla (`INJ`/`BMF`/`ABRASR`) çözdüğünden eski komutlar "Unknown ECU" veriyordu. `dashboard.js` içindeki `ECUS` dizisi artık gerçek family adlarını id olarak kullanıyor:
   `INJ, BMF, ABRASR, AIRBAG, CLIM, COMBINE, DIRECTN, HDC, BOITEVIT, SPNEU, DSG, TELEMAT, AUTORADIO, AMPLHIFI, CPL, BML, ADC, BSM, ALARME, MDP_CONDUCT, MDP_PASSAG, ECRAN_C, AIDE_STAT, PROJECTEURS`.
2. **Aktüatör komutu:** `act <ecu> <cmd>` (backend'de yoktu) → **`actuator <test_id_hex>`** (backend `cmdActuator`, bağlı ECU üzerinde çalışır).
3. **Bare hex zorunlu:** `read`/`write`/`meas` argümanları `0x` ön eki OLMANDAN alınır (`100A`, `2A00`). Dashboard `h2()` helper'ı `0x` ön ekini atar.
4. **Canlı ölçüm akışı:** backend `[LIVE] <name>: <value> <unit>` basar. `parseLogLine` bunu yakalar, `handleMeasLine` kartı + sparkline'ı günceller (birim son ekini değerden soyar).
5. **Scan sonuçları:** `[SCAN] <FAMILY> done.` / `timeout.` ve rapor satırları (`INJ   OK` / `OK  DTC` / `NO COMM`) `updateEcuStatus` ile ECU listesindeki durum noktalarını ve DTC sayısını doldurur.
6. **DTC satırları:** `  DTC <hex> - <desc> (status: <hex>)` → `addDtcToTable` ile tabloya eklenir.

### Dashboard içindeki anahtar fonksiyonlar / sözleşmeler
- `ECUS[]` — id = family adı (Bölüm 5.1).
- `FAMILY_FOR` / `familyOf(id)` — geriye dönük uyumluluk için; artık id===family olduğu için `familyOf(id)===id`.
- `ECU_CONFIG_PARAMS[family]`, `ECU_MEAS_PARAMS[family]`, `ACTUATOR_TESTS[family]` — `ecu_params.hpp`'dan üretilir, **elle düzenlenmez**.
- `sendCommand(cmd)` — fetch `'/api/cmd?val='+encodeURIComponent(cmd)`.
- `connectSSE()` — EventSource; `onmessage` → `parseLogLine` (meas tarama burada).
- **DOM id'leri (2026-07-25'te gerçek dosyalara karşı yeniden çıkarıldı).** Buradaki eski liste
  arayüzün 2026-07-18'de sıfırdan yazılmasından kalmaydı ve **31 id'nin 23'ü artık hiçbir dosyada
  yoktu** (`sseStatus statusDot statusText consoleOutput ecuList ecuDetail bsiTree bsiDetail
  measCategory btnCsvExport chkRecord measGrid actTestList dtcEcuSelect btnStartScan btnBsiReadAll
  btnBsiWrite btnActLoadTests btnToggleAutoScroll btnConsoleSend fwInput btnFwSend ecuCount` —
  hepsi silindi). Doğrulandı: `dashboard.js`'in aradığı **her** id `dashboard.html`'de mevcut,
  kırık `getElementById` yok. Güncel liste:

  `#app #sidebar #sideToggle #themeToggle #indLink #indComms #vehVin #vehModel #ecuTree #ecuName
  #ecuMeta #lockState #funcTabs #scanSummary #btnGlobalTest #btnPdi #btnIdent #identTable
  #btnUnlock #pinInput #btnReadDtc #btnClearDtc #dtcTable #dtcBody #dtcCount #measSelect
  #btnMeasStart #btnMeasStop #measName #measValue #measUnit #measSpark #actList #cfgEditor
  #cfgSearch #btnCfgReadAll #srecInput #btnFlashBegin #btnFlashSend #btnFlashEnd #btnFlashStatus
  #btnFlashCancel #sniffScenario #sniffStep #btnSniffRun #btnSniffNext #btnSniffBase #btnSniffStart
  #btnSniffStop #btnSniffClear #btnSniffSave #btnSniffLoad #btnSniffWatch #btnSniffWatchOff
  #sniffMode #sniffN #sniffWatchBus #sniffWatchId #sniffLearnedTable #sniffLearnedBody
  #sniffCandTable #sniffCandBody #sniffEmpty #console #consoleOut #consoleInput #btnConsoleToggle
  #btnClearLog #chkAutoscroll`

  **Yeni HTML yazarken bunları koru.** Doğrulama tek satır:
  `python3 -c "import re;h=set(re.findall(r'id=\"([\w-]+)\"',open('dashboard/dashboard.html').read()));print([x for x in re.findall(r'getElementById\([\x27\"]([\w-]+)',open('dashboard/dashboard.js').read()) if x not in h])"`

---

## 0b. GÜNCELLEME — Telekodlama editörü (Lexia-tarzı ayar menüsü)

Telecoding sekmesi ham-hex `prompt`'tan **Lexia-tarzı gerçek ayar menüsüne** çevrildi.
- **Tek generator (birleştirildi):** `scripts/gen_ecu_data.py` artık HEM `ecu_params.hpp`
  (24 ECU: config/meas/actuator) HEM `ecu_zones.hpp` (BSI telekodlama → `ECU_CONFIG_PARAMS['BMF']`,
  131 param, cruise control dahil) parse ediyor. `--write` ile dashboard.js'in üretilen bloğunu
  yerinde günceller (elle yazılmış app kuyruğunu korur), sonra `scripts/generate_assets.py`.
  Enum'lar dosya-kapsamlı çözülür (`kYesNo` iki dosyada farklı). Ayrık `gen_bsi_params.py` silindi.
  **Doğrulama:** birleşik generator, çalışan dashboard.js verisini 24 ECU'da **0 fark** ile
  yeniden üretiyor (semantic diff); tek yenilik BMF config'in dolması.
- NOT: `scripts/gen_ecu_data.py` zaten mevcuttu — §4/§8'deki "üretici /tmp'de kayboldu" notu
  yanlıştı; üretici `scripts/` altında ve artık BSI'ı da kapsıyor.
- **Editör (`dashboard.js`):** parametreler `category`'ye göre gruplanır (submenü), her biri
  dropdown (enum/bool) veya sayı girişi; **Read configuration** her zone'u **sıralı** okur
  (`cfgReadNext`/`cfgAdvance` — cihaz tek yanıt işlediği için watchdog'lu, 2 sn), ham byte'ları
  saklar, decode edip kontrolü doldurur.
- **Apply = oku-değiştir-yaz:** `bytes[b] = (bytes[b] & ~mask) | (val<<shift & mask)` → sadece
  ilgili bitler değişir, zone'un diğer byte'ları korunur; ardından otomatik re-read ile onay.
  Unlock zorunlu. Uçtan uca sahte-cihazla doğrulandı (örn. cruise `0100 B1→BD`).
- Ham byte'lar için firmware değişmedi: tek-zone `read` zaten ham hex basıyor
  (`[CONFIG] Zone XXXX:` + hex satırı; BSI-dışı ECU'da `[DIAG] Zone .. (N bytes): hex`).

## 6. Bilinen Eksiklikler / Sonraki Adımlar

- ~~**Config "Current value" gösterilmiyor**~~ → **ÇÖZÜLDÜ** (bkz. §0b, yeni telekodlama editörü).
- ~~**DTC/Config sekmeleri otomatik connect yapmıyor**~~ → **YANLIŞ ALARM, doğrulandı (2026-07-18):** `selectEcu()` zaten `cmdSeq("exit","connect "+id)` gönderiyor; `setConnected(false/true)` SSE'deki `"Session open with X. Ready"` satırına bağlı olarak `btnReadDtc`/`btnClearDtc`/`btnCfgReadAll` vb. butonları doğru şekilde disable/enable ediyor ([dashboard.js:984](dashboard/dashboard.js#L984), [:1000](dashboard/dashboard.js#L1000)). Not: ham konsoldan elle `dtc` yazan kullanıcı hâlâ önce `connect` yazmalı — bu beklenen davranış.
- ~~**Host test derlenmiyor**~~ → **ÇÖZÜLDÜ (2026-07-18):** PICO_SDK ile ilgisi yoktu; `tests/test_psa.cpp` başındaki derleme komutu eksikti (`diag_shell.cpp`/`flash_engine.cpp` linklenmiyordu → `undefined reference`). Doğru komut: `g++ -std=c++17 -Iinclude -DHOST_TEST tests/test_psa.cpp src/isotp.cpp src/diag_shell.cpp src/flash_engine.cpp -o test_psa && ./test_psa`. Artık **tüm testler geçiyor** (host ortamında doğrulandı, PICO_SDK_PATH gerekmiyor).
- **Ölçüm tek parametre (ÇÖZÜLEMEDİ — mimari kısıt):** backend `live_param_id_` ([diag_shell.hpp](include/psa/diag_shell.hpp)) tek bir alan; grid aynı anda birden fazla kartı canlı güncelleyemez. Çözüm için firmware'de çoklu-param polling state machine gerekir (istek/yanıt döngüsünü ECU başına birden fazla ID için sıralamak) — bu küçük bir yama değil, tasarım kararı gerektirir, kullanıcı onayı olmadan üstlenilmedi.
- **ECRAN_C / AIDE_STAT adresleri doğrulanamadı (ÇÖZÜLEMEDİ):** `ludwig-v/arduino-psa-diag` ECU_LIST.md'de bu isimler yok; mevcut adresler (0x770:0x670, 0x76E:0x66E) best-effort tahmin. Gerçek araçta CAN sniff ile doğrulanmadan güvenilmemeli — bu proje ortamından (kod/doküman) çözülemez, fiziksel donanım/araç erişimi gerekir.
- **Çoğu ECU'nun güvenlik PIN'i bilinmiyordu → KISMEN ÇÖZÜLDÜ (2026-07-18):** gerçek tedarikçi PIN veritabanı (`ludwig-v/psa-seedkey-algorithm` ECU_KEYS.md) artık **`docs/ECU_KEYS.md`'de projeye kalıcı olarak eklendi** (önceden sadece geçici bir oturum scratchpad'indeydi, kayboluyordu). Yeni ECU/model PIN'i eklerken önce buraya bak. `ecu_keys.hpp`'ye eklendi:
  - **Yüksek güven** (isim birebir eşleşti): `AIRBAG=B2DF` (SAC_AUTOLIV), `CPL=EE3E` (CDPL), `ECRAN_C=F6C4` (EMF_C).
  - **En iyi tahmin** (kaynakta birden fazla model varyantı var, KWP-çağı/C5-Mk1-FL'e en yakını seçildi — `unlock` başarısız olursa zararsız, `pin <hex>` ile ezilebilir): `BOITEVIT=8962` (AL4_AT8), `AMPLHIFI=A7D8` (AMPLI_AUDIO), `DIRECTN=BF62` (DAE), `ABRASR=ABFB` (ESP81), `DSG=AC58` (DSG_UDS, en zayıf tahmin — kaynakta sadece UDS varyantı var).
  - **Hâlâ bilinmiyor (kaynakta karşılığı yok):** CLIM, HDC, SPNEU, AUTORADIO, BML, ADC, MDP_CONDUCT, MDP_PASSAG, PROJECTEURS, AIDE_STAT — bunlar için gerçek üretici sırrı gerekiyor, kod/analizle üretilemez.
- **Kalan eski v1 dosyalar:** doğrulandı — `dashboard/index.html`, `dashboard/script.js`, `dashboard/style.css` artık **mevcut değil** (önceki oturumda silinmiş), bu madde geçersiz.
- **Lexia 3 referansı:** `docs/lexia3_menu_reference.md` artık VAR (2026-07-18'de eklendi, kaynak: Lexia 3 Part I/II/III video analizi, C5 3.0L V6 test aracı VIN VF7RCXFUJ6L502935). İçeriği `ecu_zones.hpp`/`ecu_params.hpp`'deki BSI Configuration + Customer Options + AUTORADIO Config parametreleriyle bit-bit örtüşüyor — muhtemelen bu tabloların orijinal kaynağı. İkinci referans: `docs/psa_can_reference.md`.
- **`hwtest` gerçek donanımda doğrulanmadı (bkz. §0c):** SPI-readback ve CAN-loopback testi mantığı host test'te derleniyor ama `#ifdef HOST_TEST` ile çalıştırılmıyor (gerçek MCP2515 gerektirir). Karta ilk seri bağlantıda `hwtest` çalıştırılıp HS/LS her ikisinin de PASS verdiği teyit edilmeli.

---

## 7. Build / Doğrulama

- **Dashboard değişince MUTLAKA çalıştır:**
  ```
  python3 scripts/generate_assets.py
  ```
  Bu `include/psa/dashboard_assets.h`'i yeniden üretir.
- **C++ derleme (gerçek cihaz):** CMake + PICO_SDK gerektirir. `PICO_SDK_PATH` ayarlı olmalı.
  Board artık `CMakeLists.txt`'de pinli (`pico2_w`), **`-DPICO_BOARD` vermeye gerek yok**:
  ```
  cmake -B build && cmake --build build -j8
  ```
  (2026-07-25 öncesinde board pinli değildi ve bu komut lwIP başlıkları bulunamadığı için hiç
  derlenmiyordu — bkz. §0e.)
- **Host test (komut 2026-07-25'te düzeltildi):** `src/*.cpp` glob'unu direkt derlemeye çalışma —
  `main.cpp`/`mcp2515.hpp` gerçekten Pico SDK ister. Ama `tests/test_psa.cpp` PICO SDK
  **istemiyor**. Doğru komut — eski hali `can_sniffer.cpp` ve `wifi_server.cpp`'yi atladığı için
  linklenmiyordu:
  ```
  g++ -std=c++17 -Iinclude -DHOST_TEST tests/test_psa.cpp src/isotp.cpp src/diag_shell.cpp \
      src/flash_engine.cpp src/can_sniffer.cpp src/wifi_server.cpp -o /tmp/test_psa && /tmp/test_psa
  ```
  PICO_SDK_PATH gerektirmeden temiz derlenir, 27 kontrolün hepsi geçer.
  CMake yolu da aynı dosya kümesini derler:
  ```
  cmake -B build_host -DHOST_TEST=ON && cmake --build build_host && ./build_host/test_psa
  ```
  (`tests/test_psa.cpp` başındaki yorum bloğu da bu komutla senkron.)
- **JS sözdizimi:** `node --check dashboard/dashboard.js` ile doğrulandı (geçti).

---

## 8. Hızlı Başlangıç (yeni model için)

1. Proje kökü: `/home/umut/citroen-can-interface`
2. Referans: `docs/psa_can_reference.md`, `include/psa/ecu_params.hpp` (veri otoritesi), `src/diag_shell.cpp` (komut sözleşmeleri).
3. UI değişeceksen: `dashboard/` altındaki 3 dosyayı düzenle → `python3 scripts/generate_assets.py` → CMake build.
4. Yeni ECU verisi ekleyeceksen: `ecu_params.hpp`'yı düzenle → `/tmp/gen_ecu.py` benzeri bir parser ile `dashboard.js` verisini yeniden üret (veya elle senkronize et).
5. Backend komut sözleşmesini değiştirme — dashboard buna bağımlı (Bölüm 2, 5).
