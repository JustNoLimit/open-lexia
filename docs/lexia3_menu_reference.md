# Lexia 3 Menü Sistemi Referans Dokümanı
## Open Source PSA Diagnostik Aracı Geliştirme İçin Tam Kaynak

---

## 1. Genel Bakış

### 1.1 Lexia 3 Nedir?
Lexia 3, Citroën ve Peugeot araçları için kullanılan orijinal diagnostik araçtır. KWP2000 ve UDS protokolleri üzerinden araçtaki tüm ECU'larla (Electronic Control Unit - Elektronik Kontrol Ünitesi) iletişim kurar.

### 1.2 Test Aracı Bilgileri
- **Araç:** Citroën C5 (Yeni Look / Remodele)
- **Motor:** 3.0L V6 Benzinli (Petrol)
- **Vites:** Otomatik (BVA_AM6 - AL4 tipi)
- **VIN:** VF7RCXFUJ6L502935
- **Bölge:** Asya (Asia)
- **Yakıt Deposu:** 68 litre
- **Yakıt Tipi:** Benzin (Petrol)

---

## 2. Ana Menü Yapısı

```
LEXIA 3 ANA MENÜ
├── Diagnosis (Teşhis) ← ASIL KULLANIM ALANI
├── Downloading (Yazılım İndirme)
├── Replacement Parts (Yedek Parçalar)
├── Pre-Delivery Inspection (Teslimat Öncesi Kontrol)
├── Personalised Maintenance (Kişisel Bakım)
├── Battery Charge Status (Batarya Durumu)
└── Consult Stored Data (Kayıtlı Verileri Görüntüleme)
```

---

## 3. Diagnosis (Teşhis) Akışı

### 3.1 Başlangıç Süreci
1. **Ignition OFF/ON Mesajı:** "Switch off the ignition and switch it on"
2. **Araç Yapılandırma Taraması:** "Search for the vehicle configuration" (Progress bar ile %0-100)
3. **ECU Taraması:** Tüm ECU'lar taranır ve sonuçlar tablo halinde gösterilir

### 3.2 Global Test Sonuç Tablosu

| Sütun | Açıklama |
|-------|----------|
| **system** | ECU adı |
| **dialogue** | ECU ile iletişim durumu (YES/NO) |
| **fault** | Hata kodu varlığı (YES/NO) |

**Tarama Sırası (Bizim Aracımız):**

| # | System | Dialogue | Fault |
|---|--------|----------|-------|
| 1 | BSI | YES | NO |
| 2 | Faults log | YES | YES |
| 3 | Instrument panel | YES | NO |
| 4 | Radio | YES | YES |
| 5 | CD player | YES | NO |
| 6 | Parking assistance | YES | NO |
| 7 | Air conditioning | YES | NO |
| 8 | Alarm | YES | NO |
| 9 | Switch module under the steering wheel | YES | NO |
| 10 | Driver's door module | YES | YES |
| 11 | Passenger's door module | YES | YES |
| 12 | Airbag | YES | NO |
| 13 | Multifunction screen | YES | YES |
| 14 | Injection | YES | NO |
| 15 | Engine relay unit | YES | NO |
| 16 | Anti-lock brake system | YES | NO |
| 17 | Suspension | YES | NO |
| 18 | Gearbox ECU | YES | NO |
| 19 | Directional headlamps | YES | NO |
| 20 | Deflation detection | YES | NO |

### 3.3 ECU Seçimi ve Bağlantı
Herhangi bir ECU'ya tıklandığında:
1. **"Initialisation of the dialogue with the ECU, in progress..."** (kumanda saati ikonu)
2. **"Ecu [ECU ADI] - validate to continue"** (onay isteği)
3. ECU'nun menüsü açılır

---

## 4. ECU Detay Menüleri

### 4.1 BSI (Body Systems Interface - Gövde Sistemleri Arayüzü)
**Konum:** En kritik ECU - tüm araç yapılandırması burada

#### Ana Menü
```
BSI
├── Identification (Kimlik Bilgileri)
├── Fault Reading (Hata Okuma)
├── Fault Erasing (Hata Silme)
├── Parameter Measurements (Parametre Ölçümleri)
├── Actuator Tests (Aktüatör Testleri)
├── Maintenance - BSI Operating Mode (Bakım)
├── Configuration (Yapılandırma) ← EN ÖNEMLİ BÖLÜM
└── Programming (Programlama)
```

#### 4.1.1 Configuration Menüsü
```
Configuration
├── Manual Configuration
│   ├── Customer Options (Müşteri Seçenekleri)
│   ├── Configuration (Yapılandırma)
│   │   ├── Vehicle Definition - Equipment - Driving Information
│   │   ├── Passenger Compartment Heating - Air Conditioning
│   │   ├── Lighting - Signalling - Visibility - Rear View Mirrors
│   │   ├── Locking - Doors/Windows - Immobiliser - Alarm
│   │   └── Fuel Sender Law - Oil Level Sender Law
│   ├── ECUs Present (Mevcut ECU'lar)
│   └── VIN Code (VIN Kodu)
└── Internet Configuration
```

#### 4.1.2 Customer Options (Müşteri Seçenekleri)
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Multiplexed electric door mirrors with fold in function | yes | Boolean |
| Presence of rear wiping in reverse gear | No | Boolean |
| Close windows with high frequency remote control and key option | Present | Boolean |
| Type of tyre deflection detection | Direct without display of pressures | Enum |
| Type of day running lights | No daytime lights | Enum |
| Driver's seat belt not fastened detection | yes | Boolean |

#### 4.1.3 Vehicle Definition - Equipment - Driving Information
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Overspeed warning for the Arabian peninsula | Missing | Boolean |
| Passenger's seat position memory unit option | Missing | Boolean |
| Automatic gearbox option | Present | Boolean |
| RH drive vehicle | No | Boolean |
| Dynamic stability control option (ESP) | Present | Boolean |
| Variable damping suspension option | Missing | Boolean |
| Driving school vehicle option | Missing | Boolean |
| Three door vehicle | No | Boolean |
| Oil temperature sensor option | Present | Boolean |
| Coolant level sensor option | Present | Boolean |
| Passenger airbag option | Present | Boolean |
| Presence of Telematic unit (RT3/RT4) | No | Boolean |
| Water in diesel sensor | Missing | Boolean |
| Air pump presence | No | Boolean |
| Multiplexed ABS option | Present | Boolean |
| Estate vehicle | No | Boolean |
| Presence of controlled manual gearbox | No | Boolean |
| Origin of water in fuel information | Engine management ECU | Enum |
| Driver's seat memorisation option | Missing | Boolean |
| Presence of a trailer relay unit | No | Boolean |
| Presence and type of cruise control | Cruise control and speed limitation | Enum |
| Source of oil temperature information | Engine relay unit | Enum |
| Presence of parking assistance button | yes | Boolean |
| Type of parking assistance | Front and rear | Enum |
| Presence of function log | yes | Boolean |
| Presence of warning log | yes | Boolean |
| Parking assistance with visual information | yes | Boolean |
| Parking assistance with audible information | yes | Boolean |
| Type of fuel filler cap presence detection | No detection | Enum |
| Control of the diesel additive pump | By the particle filter | Enum |
| Type of alternator | Standard | Enum |
| Engine management ECU compatible with speed limiter | yes | Boolean |
| Overtaking assistance option | Missing | Boolean |
| Type of seat belt fastening management unit | Wire | Enum |
| Total period before maintenance - month(s) | 24 | Numeric |
| Revolutions before maintenance (limit) - million(s) of revolutions | 80 | Numeric |
| First maintenance limit - Km | 2500 | Numeric |
| Maintenance limit - Km | 15000 | Numeric |
| Distance limit for forcing customer mode - Km | 250 | Numeric |
| Memorizing of faults | Authorised | Enum |
| Distance limit for automatically switching from parc mode to customer mode - Km | 10 | Numeric |
| Customisation menu type | Unique user profile | Enum |
| Presence of a secondary electric brake | No | Boolean |
| Rear seat position memory unit | absent | Boolean |
| Presence of welcome function for the driver | No | Boolean |
| Origin of oil pressure information | Engine relay unit | Enum |
| Activation of seat belt not fastened detection | yes | Boolean |
| Presence of front passenger detection area | Missing | Boolean |
| Driver's seat belt not fastened detection | yes | Boolean |
| Front passenger's seat belt not fastened detection | yes | Boolean |
| Front middle passenger's seat belt not fastened detection | No | Boolean |
| Presence of a RD4 audio system | yes | Boolean |
| Presence of a red LED for Power Steering warning | No | Boolean |
| Presence of an orange LED for power steering warning | No | Boolean |
| Tolerance on vehicle speed limitation / cruise control setting kph | 2.0 | Numeric |
| Lane departure warning system option | Missing | Boolean |
| Presence of a fuel pump | yes | Boolean |
| Presence of faulty parking assistance warning | yes | Boolean |
| Rear middle passenger's seat belt not fastened detection | yes | Boolean |
| Rear LH passenger's seat belt not fastened detection | yes | Boolean |
| Rear RH passenger's seat belt not fastened detection | yes | Boolean |
| Display of the fuel consumption without the extra consumption due to particle filter regeneration | No | Boolean |
| Response time from the engine management ECU to the BSI when the cruise control is switched on (in seconds) | 0.6 | Numeric |
| Display of the rear seat belt reminder warning when a rear door is opened or when the ignition is switched on | yes | Boolean |

#### 4.1.4 Passenger Compartment Heating - Air Conditioning
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Presence of exterior temperature sensor | yes | Boolean |
| Presence of air conditioning compressor with external control | yes | Boolean |
| Presence of pollutant sensor | No | Boolean |
| Type of sunshine sensor | Two zone sunshine sensor | Enum |
| Type of air mixing | Two zone | Enum |
| Type of air distribution | Two zone | Enum |
| Type of additional heating | absent | Enum |
| Air conditioning compressor drive ratio | 1.19 | Numeric |
| Presence of a controlled blower motor | yes | Boolean |

#### 4.1.5 Lighting - Signalling - Visibility - Rear View Mirrors
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Brightness sensor option | Missing | Boolean |
| Rain sensor option | Present | Boolean |
| Rear screen wiper option | Missing | Boolean |
| Headlamp washer option | Present | Boolean |
| Vehicle location using indicators | Missing | Boolean |
| Automatic hazard warning lamps illumination in the event of impact option | Missing | Boolean |
| Illumination of hazard warning lamps on heavy deceleration option | Present | Boolean |
| Dipped beam and main beam in the same lens unit | No | Boolean |
| Front fog lamps presence | yes | Boolean |
| Multiplexed electric door mirrors with fold back | yes | Boolean |
| Indexed mirrors for reverse gear | yes | Boolean |
| Presence of rear wiping in reverse gear | No | Boolean |
| Illumination of hazard warning lamps when emergency call button pressed | Inactive | Boolean |
| Cold climate option | Missing | Boolean |
| Main beam headlamps and fog lamps in the same lens unit option | Missing | Boolean |
| Black Panel mode option | Present | Boolean |
| Presence of directional headlamps | yes | Boolean |
| Type of front lighting | Xenon bulbs | Enum |
| Type of day running lights | No daytime lights | Enum |
| Presence of a stalk with one-touch automatic wiper activation | No | Boolean |
| Presence of a LH reversing lamp | yes | Boolean |
| Presence of a RH reversing lamp | yes | Boolean |
| Type of interior lamp switch | One-touch switch | Enum |

#### 4.1.6 Locking - Doors/Windows - Immobiliser - Alarm
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Locking when driving option | Present | Boolean |
| Mercosur electric window logic | No | Boolean |
| Central closing using the high frequency remote control | yes | Boolean |
| Type of locking | deadlocking | Enum |
| Two front multiplexed electric windows | yes | Boolean |
| Two front electric windows | yes | Boolean |
| Sunroof number / type | No sunroof | Enum |
| Opening rear screen option | Missing | Boolean |
| Child safety option | Present | Boolean |
| Close windows with high frequency remote control and key option | Present | Boolean |
| Type of child safety | Mechanical | Enum |
| Automatic relocking | yes | Boolean |
| Alarm type | Standard alarm | Enum |
| Type of key | Weak current key | Enum |
| Permanent locking of boot option | Missing | Boolean |
| Theft-proof mode | Active | Boolean |
| THATCHAM mode activation | No | Boolean |

#### 4.1.7 Fuel Sender Law - Oil Level Sender Law
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Fuel type | petrol | Enum |
| Oil level sensor option | Present | Boolean |
| Tank capacity | 68 | Numeric (litre) |
| Resistance_full - ohms | 70 | Numeric (ohm) |
| Fuel sender law - Warning level - Litres | 8.0 | Numeric (litre) |
| Fuel sender law - vehicle selection | Petrol engines | Enum |
| Dipstick law - engine | petrol 3.0L(V6) | Enum |
| Origin of oil level information | Engine relay unit | Enum |
| Oil level measuring condition | Engine off | Enum |

#### 4.1.8 Programming Menüsü
```
Programming
├── Key Programming (Anahtar Programlama)
└── BSI Initialisation (BSI Sıfırlama)
```

#### 4.1.9 Parameter Measurements (Parametre Ölçümleri)
```
Parameter Measurements
├── Standard Parameter Measurements
│   ├── Supply (Güç)
│   ├── Air Conditioning (Klima)
│   ├── Information - Driving (Sürüş Bilgisi)
│   ├── Passenger Safety (Yolcu Güvenliği)
│   ├── Lighting / Signalling (Aydınlatma / Sinyalizasyon)
│   ├── Alarm
│   ├── Engine Immobiliser (Motor Immobilizer)
│   ├── High Frequency Remote Control (Kumanda)
│   ├── Locking (Kilitleme)
│   ├── Instruments (Göstergeler)
│   ├── Doors (Kapılar)
│   └── Visibility (Görünürlük)
├── Personalised Parameter Measurements (Max 6 parametre seçimi)
│   ├── Instrumentation - measurements
│   ├── trip computer
│   ├── Instrumentation - LEDs
│   ├── Doors
│   ├── Visibility
│   ├── Main supplies
│   ├── Reasons for maintaining CANs
│   ├── Reasons for last wake-up of CANs
│   ├── Load shedding level
│   ├── High frequency remote control
│   ├── Coolant information
│   ├── Engine oil information
│   ├── Fuel information
│   ├── Mileage - consumption information
│   ├── Passenger safety
│   ├── Air conditioning
│   ├── Information - driving
│   ├── Lighting / signalling
│   ├── alarm
│   ├── engine immobiliser
│   └── locking
└── View Graphical Measurements Saved (Kaydedilmiş Grafikler)
```

**Main Supplies (Güç) Parametreleri:**
| Parametre | Değer | Birim |
|-----------|-------|-------|
| battery voltage | 12.1 | Volt(s) |
| Economy mode | Inactive | - |
| BSI operating mode | Customer mode | - |
| Presence of + ignition on (+IGN) | yes | Boolean |
| Ignition key in + cranking position | No | Boolean |
| Vehicle electrical status | Status 1 | Enum |
| Engine operating status | coupé | Enum |
| Alternator excitation voltage | 0.0 | Volt(s) |

**Fuel Information Alt Parametreleri:**
- Water in diesel
- Fuel sender impedance
- Displayed fuel level
- Gross fuel level

**Engine Oil Information Alt Parametreleri:**
- Oil pressure warning (1)
- Measured oil level (1)
- Oil temperature (1)

#### 4.1.10 Actuator Tests (Aktüatör Testleri)
```
Actuator Tests
├── Air Conditioning → No parameters available
├── Lighting / Signalling
│   ├── Horn (1) - Korna
│   ├── Dipped beam (1) - Kısa far
│   ├── Main headlamps (1) - Uzun far
│   ├── Vehicle and trailer LH side lamps (1)(2) - Sol park lambası
│   ├── Vehicle and trailer RH side lamps (1)(2) - Sağ park lambası
│   ├── Vehicle and trailer LH indicators (1)(2) - Sol sinyal
│   ├── Vehicle and trailer RH indicators (1)(2) - Sağ sinyal
│   ├── Vehicle and trailer rear fog lamps (2) - Arka sis farı
│   └── Interior lighting - İç aydınlatma
├── Locking (Kilitleme)
├── Instrumentation - LEDs
├── Doors (Kapılar)
└── Visibility (Görünürlük)
```

**Notlar:**
- (1) = Motor röle ünitesi kontrolünde
- (2) = Römork dahil
- Tüm testler "Inactive" durumda başlar

#### 4.1.11 Maintenance - BSI Operating Mode
```
Maintenance
├── Reset Maintenance Indicator (Bakım Sıfırlama)
└── Choice of Maintenance Schedule (Pre-delivery inspection)

BSI Operating Mode
└── (Detay videoyu göstermedi)
```

---

### 4.2 COMBINE (Instrument Panel - Gösterge Paneli)
```
COMBINE
├── Configuration
└── Actuator Tests
```

---

### 4.3 AUTORADIO (Radyo)
```
AUTORADIO
├── Configuration
├── Parameters
└── Activation
```

#### AUTORADIO Configuration Parametreleri
| Parametre | Değer | Tip |
|-----------|-------|-----|
| Vehicle serial number - VIN code | VF7RCXFUJ6L502935 | String |
| Usage geographical zone | Asia | Enum |
| CD player | present | Boolean |
| Fader function | activated | Boolean |
| AM frequency band | activated | Boolean |
| Volume linked to vehicle speed | absent | Boolean |
| Sound amplifier | absent | Boolean |
| Volume level correction law | Law N°1 | Enum |
| LO/DX sensitivity curve | Curve n°1 | Enum |
| Radiotext function | activated | Boolean |
| CDtext function | Deactivated | Boolean |
| Parking assistance | present | Boolean |
| Auxiliary input n°1 | Classic | Enum |
| Auxiliary input n°2 | Missing | Enum |
| Steering wheel with fixed central controls | absent | Boolean |

---

### 4.4 AIDE_STAT (CD Player / Parking Assistance)
```
AIDE_STAT
├── Configuration
├── Actuator Tests
└── Parameter Measurements
```

---

### 4.5 CLIMATISATION (Air Conditioning - Klima)
```
CLIMATISATION
├── Activation
└── Parameter Measurements
```

---

### 4.6 ALARME (Alarm)
```
ALARME
├── Configuration
├── Actuator Tests
└── Parameter Measurements
```

---

### 4.7 HDC (Switch Module Under the Steering Wheel - Direksiyon Altı Anahtar Modülü)
```
HDC
└── Activation
```

---

### 4.8 MDP_CONDUCT (Driver Door Module - Sürücü Kapı Modülü)
```
MDP_CONDUCT
└── Activation
```

---

### 4.9 MDP_PASSAG (Passenger Door Module - Yolcu Kapı Modülü)
```
MDP_PASSAG
└── Activation
```

---

### 4.10 SAC_AUTOLIV (Airbag - Hava Yastığı)
```
SAC_AUTOLIV
├── Configuration
└── (Diğer menüler videoyu göstermedi)
```

---

### 4.11 ECRAN_C (Multifunction Screen - Çok Fonksiyonlu Ekran)
```
ECRAN_C
├── Configuration
└── (Diğer menüler videoyu göstermedi)
```

---

### 4.12 ME747 (Injection - Enjeksiyon - Bosch ME7.4.7)
**Motor Yönetimi - En Kritik Motor ECU'su**

```
ME747
├── Auto-adaptation (Otomatik Uyum)
├── Parameters (Parametreler)
├── Activation (Aktivasyon)
│   ├── Downstream sensor heating (front and rear banks of cylinders)
│   ├── Injector 1
│   ├── Injector 2
│   ├── Injector 3
│   ├── Injector 4
│   ├── Injector 5
│   └── Injector 6
├── Configuration (Yapılandırma)
├── Identification (Kimlik)
├── Fault Reading (Hata Okuma)
└── History (Geçmiş)
```

---

### 4.13 BSM (Engine Relay Unit - Motor Röle Ünitesi)
```
BSM
├── Identification
└── Actuator Tests
    └── Air Conditioning
```

---

### 4.14 ESP_MK60 (Anti-lock Brake System - ABS/ESP)
```
ESP_MK60
├── Parameter Measurements
│   ├── Dynamic Information
│   ├── Relay and Brake Switch Information
│   └── Steering Wheel Angle Sensor Information
├── Configuration
│   ├── Calibration (Kalibrasyon)
│   │   └── Talimatlar:
│   │       - Tekerleklerin düz olduğundan emin olun
│   │       - Araçta 1. viteste rölantide bırakın
│   │       - Direksiyonu serbest bırakın
│   │       - "CALIBRATION" komutunu başlatın
│   │       - Hataları silin
│   └── Bleeding (Fren Havası Tahliyesi)
├── Actuator Tests
└── Fault Reading
```

---

### 4.15 SUSPENSION_ECOTECH (Suspension - Süspansiyon)
```
SUSPENSION_ECOTECH
├── Parameters (Max 6 parametre seçimi)
├── Activation (Aktüatör testleri)
├── Identification
└── Fault Reading
```

---

### 4.16 BVA_AM6 (Gearbox - Otomatik Şanzıman - AL4 Tipi)
```
BVA_AM6
├── Parameters
│   └── "Allows the Ecu's parameters to be displayed"
├── Configuration
│   ├── Read configuration...
│   ├── "Validate to continue"
│   └── "Press * to start automatic configuration"
└── Activation
    └── "Validate to start the actuator test"
```

---

### 4.17 PROJECTEURS (Directional Headlamps - Yönlendirilebilir Farlar)
```
PROJECTEURS
├── Configuration
└── (Diğer menüler videoyu göstermedi)
```

---

### 4.18 DSG (Deflation Detection - Basınç Algılama / TPMS)
```
DSG
├── Parameters
│   └── "Allows the Ecu's parameters to be displayed"
└── Activation
    └── "Allows the Ecu's parameters to be displayed"
```

---

## 5. Ortak Menü Kalıpları

### 5.1 Standart ECU Menü Yapısı
Her ECU için genellikle şu menüler bulunur:
```
[ECU ADI]
├── Identification (Kimlik Bilgileri)
├── Fault Reading (Hata Okuma)
├── Fault Erasing (Hata Silme)
├── Parameter Measurements (Parametre Ölçümleri)
├── Actuator Tests (Aktüatör Testleri)
├── Configuration (Yapılandırma)
└── Programming (Programlama) - Sadece bazı ECU'larda
```

### 5.2 Parametre Ölçümü Seçim Ekranı
- "Please select your parameters (MAX = 6). Current selection = 0"
- Her kategorinin yanında `+` butonu ile genişletme
- Checkbox ile parametre seçimi
- Seçim sonrası grafiksel gösterim

### 5.3 Aktüatör Testi Akışı
1. Test seçimi → "Validate to start the actuator test"
2. Test çalışırken → "Validate to stop activation"
3. Test tamamlanır

### 5.4 Yapılandırma (Configuration) Akışı
1. "Read configuration..." (Yapılandırma okunuyor)
2. Parametre listesi görüntülenir
3. Değişiklik için "Press * to change the status of the parameter"
4. "Press * to start automatic configuration" (Otomatik yapılandırma)

### 5.5 Kalibrasyon Akışı (ESP/ABS için)
1. Araç düz zeminde, tekerlekler düz
2. Motor çalışır durumda, 1. viteste
3. Direksiyon serbest bırakılır
4. "CALIBRATION" komutu başlatılır
5. Hatalar silinir
6. Prosedür tamamlanır

---

## 6. Teknik Referanslar

### 6.1 Kullanılan Protokoller
- **KWP2000** (Keyword Protocol 2000) - Eski ECU'lar için
- **UDS** (Unified Diagnostic Services) - Yeni ECU'lar için
- **CAN Bus** (Controller Area Network) - İletişim altyapısı

### 6.2 Bağlantı Noktaları
- **OBD2 Portu:** Direksiyonun altı, sol tarafta
- **Diagnostik Arayüz:** Lexia 3 cihazı + bilgisayar

### 6.3 Hata Kodları
- Her ECU kendi hata kodlarını saklar
- "Fault reading" ile okunur
- "Fault erasing" ile silinir
- Hata kodları DTC (Diagnostic Trouble Code) formatındadır

### 6.4 ECU İsimleri ve Karşılıkları
| Lexia 3 Adı | Gerçek ECU | Açıklama |
|--------------|------------|----------|
| BSI | Body Systems Interface | Gövde elektroniği ana beyin |
| COMBINE | Instrument Cluster | Gösterge paneli |
| AUTORADIO | Head Unit | Radyo/multimedya |
| CLIMATISATION | HVAC Control | Klima kontrol |
| ALARME | Alarm ECU | Alarm sistemi |
| HDC | Steering Column Module | Direksiyon sütunu modülü |
| MDP_CONDUCT | Driver Door Module | Sürücü kapı modülü |
| MDP_PASSAG | Passenger Door Module | Yolcu kapı modülü |
| SAC_AUTOLIV | Airbag ECU | Hava yastığı kontrol |
| ECRAN_C | Multifunction Display | Ekran |
| ME747 | Engine ECU (Bosch ME7.4.7) | Motor yönetimi |
| BSM | Engine Relay Box | Motor röle kutusu |
| ESP_MK60 | ABS/ESP (Bosch MK60) | ABS/ESP kontrol |
| SUSPENSION_ECOTECH | Suspension ECU | Süspansiyon kontrol |
| BVA_AM6 | Gearbox ECU (AL4) | Şanzıman kontrol |
| PROJECTEURS | Headlamp ECU | Far kontrol |
| DSG | TPMS | Lastik basıncı algılama |

---

## 7. Open Source Proje İçin Notlar

### 7.1 Öncelik Sıralaması
1. **BSI** - En kapsamlı, tüm yapılandırma burada
2. **ME747** - Motor yönetimi, kritik
3. **ESP_MK60** - Güvenlik sistemi
4. **BVA_AM6** - Şanzıman kontrolü
5. **Diğerleri** - Daha basit menülere sahip

### 7.2 Geliştirme İpuçları
- Her ECU'nun kendine ait KWP2000/UDS adresi var
- Parametre okuma için "Read Data by Identifier" komutu kullanılır
- Aktüatör testleri için "Input/Output Control" komutu kullanılır
- Yapılandırma için "Write Data by Identifier" komutu kullanılır
- Hata okuma için "Read DTC Information" komutu kullanılır

### 7.3 Test Araçları
- **ELM327** + **OBD2** - Temel tarama için
- **Lexia 3 Clone** - Tam erişim için (dikkatli kullanılmalı)
- **OpenDiag** - Open source alternatif (kapsamlı değil)
- **pyOBD** - Python kütüphanesi

### 7.4 Riskli İşlemler
- **Programming** (BSI) - Yanlış yapılırsa araç çalışmaz
- **Configuration** - Yanlış yapılandırma sorunlara yol açar
- **Calibration** (ESP) - Doğru yapılmalı
- **Key Programming** - Güvenlik gerektirir

---

## 8. Dosya Yapısı Önerisi

```
lexia3-open-source/
├── README.md
├── docs/
│   ├── menu_reference.md (bu dosya)
│   ├── ecu_list.md
│   ├── parameters.md
│   └── protocols.md
├── src/
│   ├── main.py
│   ├── kwp2000/
│   │   ├── __init__.py
│   │   ├── protocol.py
│   │   └── commands.py
│   ├── uds/
│   │   ├── __init__.py
│   │   ├── protocol.py
│   │   └── services.py
│   ├── ecu/
│   │   ├── __init__.py
│   │   ├── bsi.py
│   │   ├── me747.py
│   │   ├── esp.py
│   │   └── ...
│   ├── ui/
│   │   ├── __init__.py
│   │   ├── main_window.py
│   │   └── ...
│   └── utils/
│       ├── __init__.py
│       └── ...
├── tests/
└── requirements.txt
```

---

## 9. Sürüm Geçmişi

- **v1.0** - İlk oluşturma (Tüm menüler belgelendi)
- Tarih: 2026-07-15
- Kaynak: Lexia 3 Part I, II, III videoları analiz edildi

---

## 10. Telif Hakkı ve Kullanım

Bu doküman open source proje geliştirme amaçlıdır. Lexia 3, Citroën/PSA Group'un tescilli yazılımıdır. Bu doküman sadece eğitim ve geliştirme amaçlı kullanılmalıdır.

---

**Not:** Bu doküman, Lexia 3'ün tüm menü yapısını ve parametrelerini kapsamaktadır. Gelecekte diğer ECU'lar için de güncellenebilir.
