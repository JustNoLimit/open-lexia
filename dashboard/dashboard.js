/* Auto-generated from include/psa/ecu_params.hpp + ecu_zones.hpp — do not edit by hand. */
var ECU_CONFIG_PARAMS = { default:{ label:'Configuration', params:[] } };
var ECU_MEAS_PARAMS   = { default:{ label:'Measurements', params:[] } };
var ACTUATOR_TESTS    = {};

ECU_CONFIG_PARAMS['BMF'] = {
  label:'BMF',
  params:[
    {name:'Overspeed warning for the Arabian peninsula', zone:0x100, byte:0, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Passenger seat position memory option', zone:0x100, byte:0, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Automatic gearbox option', zone:0x100, byte:0, mask:0x04, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'RH drive vehicle', zone:0x100, byte:0, mask:0x08, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Dynamic stability control option (ESP)', zone:0x100, byte:0, mask:0x10, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Variable damping suspension option', zone:0x100, byte:0, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Driving school vehicle option', zone:0x100, byte:0, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Three door vehicle', zone:0x100, byte:0, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Oil temperature sensor option', zone:0x100, byte:1, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Coolant level sensor option', zone:0x100, byte:1, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Passenger airbag option', zone:0x100, byte:1, mask:0x04, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Presence of Telematic unit (RT3/RT4)', zone:0x100, byte:1, mask:0x08, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Water in diesel sensor', zone:0x100, byte:1, mask:0x10, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Air pump presence', zone:0x100, byte:1, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Multiplexed ABS option', zone:0x100, byte:1, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Estate vehicle', zone:0x100, byte:1, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of controlled manual gearbox', zone:0x100, byte:2, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Driver seat memorisation option', zone:0x100, byte:2, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Presence of a trailer relay unit', zone:0x100, byte:2, mask:0x04, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence and type of cruise control', zone:0x100, byte:2, mask:0x0C, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=No cruise control','1=Cruise control only','2=Speed limiter only','3=Cruise control and speed limitation']},
    {name:'Type of parking assistance', zone:0x100, byte:2, mask:0x30, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=No parking assistance','1=Rear only (ultrasonic)','2=Front and rear (ultrasonic)','3=360 camera']},
    {name:'Parking assistance front/rear', zone:0x100, byte:2, mask:0xC0, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Rear only','1=Front and rear','2=Front/rear/side']},
    {name:'Type of fuel filler cap presence detection', zone:0x100, byte:3, mask:0x01, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=No detection','1=Switch type','2=Resistive type']},
    {name:'Control of the diesel additive pump', zone:0x100, byte:3, mask:0x06, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=By the particle filter','2=By the injection ECU']},
    {name:'Type of alternator', zone:0x100, byte:3, mask:0x18, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Standard alternator','1=Alternator with regulator','2=Smart alternator (Li-ion)','3=Alternator-starter hybrid']},
    {name:'Engine management ECU compatible with speed limiter', zone:0x100, byte:3, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Overtaking assistance option', zone:0x100, byte:3, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Presence of a secondary electric brake', zone:0x100, byte:3, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Origin of water in fuel information', zone:0x101, byte:0, mask:0x01, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=Engine management ECU','2=BSI','3=Dedicated sensor']},
    {name:'Source of oil temperature information', zone:0x101, byte:0, mask:0x06, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=Engine relay unit','2=Engine management ECU','3=Dedicated sensor']},
    {name:'Type of seat belt fastening management unit', zone:0x101, byte:0, mask:0x18, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=Wire','2=Multiplexed']},
    {name:'Presence of parking assistance button', zone:0x101, byte:0, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of parking assistance warning', zone:0x101, byte:0, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of a RD4 audio system', zone:0x101, byte:0, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Parking assistance with visual information', zone:0x101, byte:1, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Parking assistance with audible information', zone:0x101, byte:1, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of function log', zone:0x101, byte:1, mask:0x04, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of warning log', zone:0x101, byte:1, mask:0x08, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of a fuel pump', zone:0x101, byte:1, mask:0x10, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of front passenger detection area', zone:0x101, byte:1, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Presence of welcome function for the driver', zone:0x101, byte:1, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of faulty parking assistance warning', zone:0x101, byte:1, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Activation of seat belt not fastened detection', zone:0x101, byte:2, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Driver seat belt not fastened detection', zone:0x101, byte:2, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Front passenger seat belt not fastened detection', zone:0x101, byte:2, mask:0x04, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Front middle passenger seat belt not fastened detection', zone:0x101, byte:2, mask:0x08, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Rear middle passenger seat belt not fastened detection', zone:0x101, byte:2, mask:0x10, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Rear LH passenger seat belt not fastened detection', zone:0x101, byte:2, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Rear RH passenger seat belt not fastened detection', zone:0x101, byte:2, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Display of rear seat belt reminder on door open', zone:0x101, byte:2, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Origin of oil pressure information', zone:0x101, byte:3, mask:0x01, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=Engine relay unit','2=Engine management ECU']},
    {name:'Origin of oil level information', zone:0x101, byte:3, mask:0x06, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not present','1=Engine relay unit','2=Engine management ECU','3=Dedicated sensor']},
    {name:'Customisation menu type', zone:0x101, byte:3, mask:0x08, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Standard user profile','1=Unique user profile','2=Customised user profile']},
    {name:'Memorizing of faults', zone:0x101, byte:3, mask:0x10, category:'Vehicle Definition', type:'ZT_ENUM', enumVals:['0=Not authorised','1=Authorised','2=Authorised with warning']},
    {name:'Lane departure warning system option', zone:0x101, byte:3, mask:0x20, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Rear seat position memory unit', zone:0x101, byte:3, mask:0x40, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Display fuel consumption without DPF regen extra', zone:0x101, byte:3, mask:0x80, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Total period before maintenance (months)', zone:0x102, byte:0, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Revolutions before maintenance (millions)', zone:0x102, byte:1, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'First maintenance limit (km) /100', zone:0x102, byte:2, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Maintenance limit (km) /100', zone:0x102, byte:3, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Distance limit for forcing customer mode (km)', zone:0x102, byte:4, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Distance limit for parc to customer mode switch (km)', zone:0x102, byte:5, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Tolerance on speed limitation/cruise setting (kph*10)', zone:0x102, byte:6, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Response time from ECU to BSI for cruise (s*10)', zone:0x102, byte:7, mask:0xFF, category:'Vehicle Definition', type:'ZT_NUMERIC', enumVals:null},
    {name:'Red LED for Power Steering warning', zone:0x103, byte:0, mask:0x01, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Orange LED for Power Steering warning', zone:0x103, byte:0, mask:0x02, category:'Vehicle Definition', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Multiplexed electric door mirrors with fold in function', zone:0x2100, byte:0, mask:0x01, category:'Customer Options', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of rear wiping in reverse gear', zone:0x2100, byte:0, mask:0x02, category:'Customer Options', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Close windows with high frequency remote control and key', zone:0x2100, byte:0, mask:0x04, category:'Customer Options', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Type of tyre deflation detection', zone:0x2100, byte:0, mask:0x08, category:'Customer Options', type:'ZT_ENUM', enumVals:['0=Not present','1=Indirect (ABS-based) without display of pressures','2=Direct (pressure sensors) with display','3=Indirect with display']},
    {name:'Type of day running lights', zone:0x2100, byte:0, mask:0x30, category:'Customer Options', type:'ZT_ENUM', enumVals:['0=No daytime lights','1=Dipped beam DRL','2=Dedicated DRL lamps','3=LED DRL','4=Position lamps DRL']},
    {name:'Driver seat belt not fastened detection', zone:0x2100, byte:0, mask:0x40, category:'Customer Options', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of exterior temperature sensor', zone:0x2300, byte:0, mask:0x01, category:'Heating/AC', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of AC compressor with external control', zone:0x2300, byte:0, mask:0x02, category:'Heating/AC', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of pollutant sensor', zone:0x2300, byte:0, mask:0x04, category:'Heating/AC', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Type of sunshine sensor', zone:0x2300, byte:0, mask:0x08, category:'Heating/AC', type:'ZT_ENUM', enumVals:['0=Not present','1=Single zone sunshine sensor','2=Two zone sunshine sensor']},
    {name:'Type of air mixing', zone:0x2300, byte:0, mask:0x30, category:'Heating/AC', type:'ZT_ENUM', enumVals:['0=Manual mixing','1=Automatic mixing (single zone)','2=Two zone','3=Tri-zone']},
    {name:'Type of air distribution', zone:0x2300, byte:0, mask:0xC0, category:'Heating/AC', type:'ZT_ENUM', enumVals:['0=Manual distribution','1=Automatic distribution','2=Two zone']},
    {name:'Type of additional heating', zone:0x2300, byte:1, mask:0x07, category:'Heating/AC', type:'ZT_ENUM', enumVals:['0=Absent','1=Electric PTC heater','2=Fuel-burning heater','3=Electric + fuel heater']},
    {name:'AC compressor drive ratio (/100)', zone:0x2300, byte:1, mask:0xF8, category:'Heating/AC', type:'ZT_NUMERIC', enumVals:null},
    {name:'Presence of a controlled blower motor', zone:0x2300, byte:2, mask:0x01, category:'Heating/AC', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Brightness sensor option', zone:0x2200, byte:0, mask:0x01, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Rain sensor option', zone:0x2200, byte:0, mask:0x02, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Rear screen wiper option', zone:0x2200, byte:0, mask:0x04, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Headlamp washer option', zone:0x2200, byte:0, mask:0x08, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Automatic hazard warning lamps illumination on impact', zone:0x2200, byte:0, mask:0x10, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Front fog lamps presence', zone:0x2200, byte:0, mask:0x20, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Multiplexed electric door mirrors with fold back', zone:0x2200, byte:0, mask:0x40, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Indexed mirrors for reverse gear', zone:0x2200, byte:0, mask:0x80, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of directional headlamps', zone:0x2200, byte:1, mask:0x01, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Type of front lighting', zone:0x2200, byte:1, mask:0x0E, category:'Lighting', type:'ZT_ENUM', enumVals:['0=Halogen','1=Xenon bulbs (HID)','2=LED','3=Matrix LED','4=Laser LED']},
    {name:'Type of day running lights', zone:0x2200, byte:1, mask:0x70, category:'Lighting', type:'ZT_ENUM', enumVals:['0=No daytime lights','1=Dipped beam DRL','2=Dedicated DRL lamps','3=LED DRL','4=Position lamps DRL']},
    {name:'Black Panel mode option', zone:0x2200, byte:1, mask:0x80, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Vehicle location using indicators', zone:0x2200, byte:2, mask:0x01, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Illumination of hazard on heavy deceleration', zone:0x2200, byte:2, mask:0x02, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Dipped beam and main beam in same lens unit', zone:0x2200, byte:2, mask:0x04, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of rear wiping in reverse gear', zone:0x2200, byte:2, mask:0x08, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Illumination of hazard when emergency call pressed', zone:0x2200, byte:2, mask:0x10, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Cold climate option', zone:0x2200, byte:2, mask:0x20, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Main beam and fog lamps in same lens unit', zone:0x2200, byte:2, mask:0x40, category:'Lighting', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Stalk with one-touch automatic wiper activation', zone:0x2200, byte:2, mask:0x80, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of LH reversing lamp', zone:0x2200, byte:3, mask:0x01, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Presence of RH reversing lamp', zone:0x2200, byte:3, mask:0x02, category:'Lighting', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Type of interior lamp switch', zone:0x2200, byte:3, mask:0x0C, category:'Lighting', type:'ZT_ENUM', enumVals:['0=Standard switch','1=One-touch switch']},
    {name:'Locking when driving option', zone:0x2400, byte:0, mask:0x01, category:'Locking', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Mercosur electric window logic', zone:0x2400, byte:0, mask:0x02, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Central closing using HF remote control', zone:0x2400, byte:0, mask:0x04, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Type of locking', zone:0x2400, byte:0, mask:0x0C, category:'Locking', type:'ZT_ENUM', enumVals:['0=Central locking','1=Selective unlocking (driver door first)','2=Deadlocking','3=Selective + deadlocking']},
    {name:'Two front multiplexed electric windows', zone:0x2400, byte:0, mask:0x10, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Sunroof number / type', zone:0x2400, byte:0, mask:0x60, category:'Locking', type:'ZT_ENUM', enumVals:['0=No sunroof','1=Manual sunroof','2=Electric sunroof','3=Panoramic roof / sunroof']},
    {name:'Type of child safety', zone:0x2400, byte:0, mask:0x80, category:'Locking', type:'ZT_ENUM', enumVals:['0=Manual child locks','1=Mechanical','2=Electric child locks']},
    {name:'Automatic relocking', zone:0x2400, byte:1, mask:0x01, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Alarm type', zone:0x2400, byte:1, mask:0x06, category:'Locking', type:'ZT_ENUM', enumVals:['0=No alarm','1=Basic alarm (perimeter)','2=Standard alarm','3=Full alarm (perimeter + volumetric)','4=Alarm with tilt sensor']},
    {name:'Type of key', zone:0x2400, byte:1, mask:0x18, category:'Locking', type:'ZT_ENUM', enumVals:['0=Standard key','1=Plip key (infrared)','2=Weak current key (RF remote)','3=Hands-free keyless entry']},
    {name:'Theft-proof mode', zone:0x2400, byte:1, mask:0x20, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'THATCHAM mode activation', zone:0x2400, byte:1, mask:0x40, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Permanent locking of boot option', zone:0x2400, byte:1, mask:0x80, category:'Locking', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Two front electric windows', zone:0x2401, byte:0, mask:0x01, category:'Locking', type:'ZT_BOOL', enumVals:['0=No','1=Yes']},
    {name:'Opening rear screen option', zone:0x2401, byte:0, mask:0x02, category:'Locking', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Child safety option', zone:0x2401, byte:0, mask:0x04, category:'Locking', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Close windows with remote and key', zone:0x2401, byte:0, mask:0x08, category:'Locking', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Fuel type', zone:0x2500, byte:0, mask:0x07, category:'Fuel/Oil', type:'ZT_ENUM', enumVals:['0=Petrol (unleaded)','1=Diesel','2=LPG','3=Petrol + LPG','4=Electric','5=Hybrid']},
    {name:'Oil level sensor option', zone:0x2500, byte:0, mask:0x08, category:'Fuel/Oil', type:'ZT_BOOL', enumVals:['0=Absent / Missing','1=Present']},
    {name:'Tank capacity (litres)', zone:0x2500, byte:1, mask:0xFF, category:'Fuel/Oil', type:'ZT_NUMERIC', enumVals:null},
    {name:'Fuel sender warning level (litres)', zone:0x2500, byte:2, mask:0xFF, category:'Fuel/Oil', type:'ZT_NUMERIC', enumVals:null},
    {name:'Oil level measuring condition', zone:0x2500, byte:3, mask:0x03, category:'Fuel/Oil', type:'ZT_ENUM', enumVals:['0=Not measured','1=Engine off','2=Measured at ignition on','3=Measured periodically','4=Measured on request only']},
    {name:'Fuel sender resistance at full (ohms)', zone:0x2501, byte:0, mask:0xFF, category:'Fuel/Oil', type:'ZT_NUMERIC', enumVals:null},
    {name:'Fuel sender law - vehicle selection', zone:0x2501, byte:1, mask:0x07, category:'Fuel/Oil', type:'ZT_ENUM', enumVals:['0=Not present','1=Diesel engines','2=Petrol engines','3=All engines']},
    {name:'Dipstick law - engine', zone:0x2501, byte:1, mask:0xF8, category:'Fuel/Oil', type:'ZT_ENUM', enumVals:['0=Not present','1=petrol 1.8L(EW7)','2=petrol 2.0L(EW10)','3=petrol 3.0L(ES9)','4=petrol 3.0L(V6)','5=diesel 1.6L(DV6)','6=diesel 2.0L(DW10)','7=diesel 2.2L(DW12)','8=diesel 2.7L(DT17)','9=diesel 3.0L(DT20)']},
    {name:'Origin of oil level information', zone:0x2501, byte:2, mask:0x01, category:'Fuel/Oil', type:'ZT_ENUM', enumVals:['0=Not present','1=Engine relay unit','2=Engine management ECU','3=Dedicated sensor']},
  ]
};
ECU_MEAS_PARAMS['BMF'] = {
  label:'BMF',
  params:[
  ]
};
ACTUATOR_TESTS['BMF'] = [
  {id:'3101', name:'Horn Test', desc:'BSI horn relay activation'},
  {id:'3102', name:'Low Beam Test', desc:'Low beam headlamp relay'},
  {id:'3103', name:'High Beam Test', desc:'High beam headlamp relay'},
  {id:'3104', name:'Left Turn Signal', desc:'Left indicator relay'},
  {id:'3105', name:'Right Turn Signal', desc:'Right indicator relay'},
  {id:'3106', name:'Fog Lights', desc:'Front fog lamp relay'},
  {id:'3107', name:'Wipers Test', desc:'Windscreen wiper relay'},
  {id:'3108', name:'Door Locking', desc:'Central door locking (lock)'},
  {id:'3109', name:'Door Unlocking', desc:'Central door locking (unlock)'},
];

ECU_CONFIG_PARAMS['INJ'] = {
  label:'INJ',
  params:[
    {name:'Throttle type', zone:0x2B00, byte:0, mask:0x01, category:'Injection Config', type:'ZT_ENUM', enumVals:['0=Mechanical','1=Electronic (E-Gas)']},
    {name:'Oxygen sensor type', zone:0x2B00, byte:0, mask:0x02, category:'Injection Config', type:'ZT_ENUM', enumVals:['0=Not present','1=Single sensor','2=Dual sensor (pre+post cat)','3=Broadband LSU']},
    {name:'Fuel system type', zone:0x2B00, byte:0, mask:0x0C, category:'Injection Config', type:'ZT_ENUM', enumVals:['0=Single point','1=Multipoint','2=Direct injection','3=Combined direct+port']},
    {name:'Turbocharger present', zone:0x2B00, byte:0, mask:0x10, category:'Injection Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'EGR valve present', zone:0x2B00, byte:0, mask:0x20, category:'Injection Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Variable valve timing', zone:0x2B00, byte:0, mask:0x40, category:'Injection Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['INJ'] = {
  label:'INJ',
  params:[
    {name:'Engine RPM', unit:'rpm', did:0x100A},
    {name:'Coolant Temperature', unit:'°C', did:0x100B},
    {name:'Battery Voltage', unit:'V', did:0x100C},
    {name:'Throttle Position', unit:'%', did:0x100D},
    {name:'Calculated Load', unit:'%', did:0x100E},
    {name:'Intake Air Temperature', unit:'°C', did:0x100F},
    {name:'Fuel Pressure', unit:'bar', did:0x1010},
    {name:'Oxygen Sensor Voltage', unit:'V', did:0x1011},
    {name:'Ignition Timing Advance', unit:'°', did:0x1012},
    {name:'Injection Timing', unit:'ms', did:0x1013},
    {name:'Idle Speed Setpoint', unit:'rpm', did:0x1014},
    {name:'Idle Speed Actual', unit:'rpm', did:0x1015},
    {name:'Intake Manifold Pressure', unit:'mbar', did:0x1016},
    {name:'Mass Air Flow', unit:'g/s', did:0x1017},
  ]
};
ACTUATOR_TESTS['INJ'] = [
  {id:'3101', name:'Injector 1', desc:'Cylinder 1 fuel injector activation'},
  {id:'3102', name:'Injector 2', desc:'Cylinder 2 fuel injector activation'},
  {id:'3103', name:'Injector 3', desc:'Cylinder 3 fuel injector activation'},
  {id:'3104', name:'Injector 4', desc:'Cylinder 4 fuel injector activation'},
  {id:'3105', name:'Injector 5', desc:'Cylinder 5 fuel injector activation'},
  {id:'3106', name:'Injector 6', desc:'Cylinder 6 fuel injector activation'},
  {id:'3107', name:'Fuel Pump Relay', desc:'Fuel pump relay activation'},
  {id:'3108', name:'Cooling Fan Low', desc:'Engine cooling fan - low speed'},
  {id:'3109', name:'Cooling Fan High', desc:'Engine cooling fan - high speed'},
  {id:'310A', name:'EGR Valve', desc:'EGR valve solenoid activation'},
  {id:'310B', name:'EVAP Canister Purge', desc:'EVAP canister purge valve'},
  {id:'310C', name:'Oxygen Sensor Heater', desc:'O2 sensor heater activation'},
];

ECU_CONFIG_PARAMS['ABRASR'] = {
  label:'ABRASR',
  params:[
    {name:'ESP function enabled', zone:0x2C00, byte:0, mask:0x01, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Traction control (ASR)', zone:0x2C00, byte:0, mask:0x02, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Hill start assist', zone:0x2C00, byte:0, mask:0x04, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Brake assist', zone:0x2C00, byte:0, mask:0x08, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Brake switch type', zone:0x2C00, byte:0, mask:0x10, category:'ESP Config', type:'ZT_ENUM', enumVals:['0=Standard switch','1=Redundant switch','2=Hall effect sensor']},
    {name:'Rollover mitigation', zone:0x2C00, byte:0, mask:0x20, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Trailer stability assist', zone:0x2C00, byte:0, mask:0x40, category:'ESP Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['ABRASR'] = {
  label:'ABRASR',
  params:[
    {name:'Wheel Speed FL', unit:'km/h', did:0x80},
    {name:'Wheel Speed FR', unit:'km/h', did:0x81},
    {name:'Wheel Speed RL', unit:'km/h', did:0x82},
    {name:'Wheel Speed RR', unit:'km/h', did:0x83},
    {name:'Steering Wheel Angle', unit:'°', did:0x84},
    {name:'Brake Switch', unit:'', did:0x85},
    {name:'Brake Pressure', unit:'bar', did:0x86},
    {name:'Longitudinal Acceleration', unit:'m/s²', did:0x87},
    {name:'Lateral Acceleration', unit:'m/s²', did:0x88},
    {name:'Yaw Rate', unit:'°/s', did:0x89},
  ]
};
ACTUATOR_TESTS['ABRASR'] = [
  {id:'3201', name:'ABS Pump Motor', desc:'Hydraulic pump motor activation'},
  {id:'3202', name:'Left Front Inlet Valve', desc:'LF brake caliper inlet valve'},
  {id:'3203', name:'Left Front Outlet Valve', desc:'LF brake caliper outlet valve'},
  {id:'3204', name:'Right Front Inlet Valve', desc:'RF brake caliper inlet valve'},
  {id:'3205', name:'Right Front Outlet Valve', desc:'RF brake caliper outlet valve'},
  {id:'3206', name:'Left Rear Inlet Valve', desc:'LR brake caliper inlet valve'},
  {id:'3207', name:'Left Rear Outlet Valve', desc:'LR brake caliper outlet valve'},
  {id:'3208', name:'Right Rear Inlet Valve', desc:'RR brake caliper inlet valve'},
  {id:'3209', name:'Right Rear Outlet Valve', desc:'RR brake caliper outlet valve'},
];

ECU_CONFIG_PARAMS['AIRBAG'] = {
  label:'AIRBAG',
  params:[
    {name:'Passenger airbag', zone:0x3000, byte:0, mask:0x01, category:'SRS Config', type:'ZT_ENUM', enumVals:['0=Activated','1=Deactivated']},
    {name:'Curtain airbags', zone:0x3000, byte:0, mask:0x02, category:'SRS Config', type:'ZT_ENUM', enumVals:['0=Not present','1=Present']},
    {name:'Seat belt reminder (driver)', zone:0x3000, byte:0, mask:0x04, category:'SRS Config', type:'ZT_ENUM', enumVals:['0=Not present','1=Belt reminder present']},
    {name:'Seat belt reminder (passenger)', zone:0x3000, byte:0, mask:0x08, category:'SRS Config', type:'ZT_ENUM', enumVals:['0=Not present','1=Belt reminder present']},
    {name:'Knee airbag (driver)', zone:0x3000, byte:0, mask:0x10, category:'SRS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Side airbags', zone:0x3000, byte:0, mask:0x20, category:'SRS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Sensor type', zone:0x3000, byte:0, mask:0x40, category:'SRS Config', type:'ZT_NUMERIC', enumVals:null},
  ]
};
ECU_MEAS_PARAMS['AIRBAG'] = {
  label:'AIRBAG',
  params:[
    {name:'Driver seatbelt status', unit:'', did:0xE0},
    {name:'Passenger seatbelt status', unit:'', did:0xE1},
    {name:'System status', unit:'', did:0xE2},
    {name:'Passenger airbag deactivation', unit:'', did:0xE3},
  ]
};
ACTUATOR_TESTS['AIRBAG'] = [
];

ECU_CONFIG_PARAMS['CLIM'] = {
  label:'CLIM',
  params:[
  ]
};
ECU_MEAS_PARAMS['CLIM'] = {
  label:'CLIM',
  params:[
    {name:'Interior Temperature', unit:'°C', did:0xD0},
    {name:'Exterior Temperature', unit:'°C', did:0xD1},
    {name:'Evaporator Temperature', unit:'°C', did:0xD2},
    {name:'Coolant Temperature (heater)', unit:'°C', did:0xD3},
    {name:'AC Pressure', unit:'bar', did:0xD4},
    {name:'Blower Speed', unit:'', did:0xD5},
    {name:'Air Distribution Position', unit:'', did:0xD6},
    {name:'Recirculation State', unit:'', did:0xD7},
    {name:'AC Compressor State', unit:'', did:0xD8},
    {name:'Sunshine Sensor', unit:'W/m²', did:0xD9},
  ]
};
ACTUATOR_TESTS['CLIM'] = [
  {id:'3601', name:'AC Compressor', desc:'Air conditioning compressor clutch'},
  {id:'3602', name:'Blower Motor', desc:'Cabin blower motor activation'},
  {id:'3603', name:'Recirculation Flap', desc:'Air recirculation flap'},
  {id:'3604', name:'Air Distribution Flap', desc:'Air distribution flap'},
  {id:'3605', name:'Heater Valve', desc:'Heater coolant valve'},
];

ECU_CONFIG_PARAMS['COMBINE'] = {
  label:'COMBINE',
  params:[
    {name:'Distance unit', zone:0x2F00, byte:0, mask:0x01, category:'Cluster Config', type:'ZT_BOOL', enumVals:null},
    {name:'Temperature unit', zone:0x2F00, byte:0, mask:0x02, category:'Cluster Config', type:'ZT_BOOL', enumVals:null},
    {name:'Speed display unit', zone:0x2F00, byte:0, mask:0x04, category:'Cluster Config', type:'ZT_BOOL', enumVals:null},
    {name:'Language', zone:0x2F00, byte:0, mask:0x08, category:'Cluster Config', type:'ZT_NUMERIC', enumVals:null},
  ]
};
ECU_MEAS_PARAMS['COMBINE'] = {
  label:'COMBINE',
  params:[
    {name:'Vehicle Speed', unit:'km/h', did:0xC0},
    {name:'Total Mileage', unit:'km', did:0xC1},
    {name:'Odometer', unit:'km', did:0xC2},
    {name:'Fuel Level', unit:'L', did:0xC3},
    {name:'Engine Temp', unit:'°C', did:0xC4},
    {name:'Ambient Temp', unit:'°C', did:0xC5},
  ]
};
ACTUATOR_TESTS['COMBINE'] = [
  {id:'3501', name:'Speedometer Sweep', desc:'Full scale speedometer gauge sweep'},
  {id:'3502', name:'Tachometer Sweep', desc:'Full scale tachometer gauge sweep'},
  {id:'3503', name:'Indicator LED Test', desc:'All indicator lamps test'},
  {id:'3504', name:'Segment Test', desc:'LCD segment display test'},
  {id:'3505', name:'Buzzer Test', desc:'Audible buzzer test'},
];

ECU_CONFIG_PARAMS['DIRECTN'] = {
  label:'DIRECTN',
  params:[
    {name:'Steering type', zone:0x3100, byte:0, mask:0x01, category:'EPS Config', type:'ZT_ENUM', enumVals:['0=Hydraulic','1=Electric (EPS)','2=Electro-hydraulic']},
    {name:'Assistance map', zone:0x3100, byte:0, mask:0x02, category:'EPS Config', type:'ZT_ENUM', enumVals:['0=Standard','1=Sport','2=Comfort','3=Automatic']},
    {name:'Variable assist (speed)', zone:0x3100, byte:0, mask:0x04, category:'EPS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Return-to-centre active', zone:0x3100, byte:0, mask:0x08, category:'EPS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Parking assist steering', zone:0x3100, byte:0, mask:0x10, category:'EPS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['DIRECTN'] = {
  label:'DIRECTN',
  params:[
    {name:'Steering Wheel Angle', unit:'°', did:0xF0},
    {name:'Steering Torque', unit:'Nm', did:0xF1},
    {name:'Motor Current', unit:'A', did:0xF2},
    {name:'Assistance Level', unit:'%', did:0xF3},
    {name:'Motor Temperature', unit:'°C', did:0xF4},
    {name:'Steering Speed', unit:'rpm', did:0xF5},
  ]
};
ACTUATOR_TESTS['DIRECTN'] = [
  {id:'3801', name:'Steering Calibration', desc:'Re-calibrate steering angle sensor'},
  {id:'3802', name:'Motor Test', desc:'Power steering motor activation test'},
];

ECU_CONFIG_PARAMS['HDC'] = {
  label:'HDC',
  params:[
  ]
};
ECU_MEAS_PARAMS['HDC'] = {
  label:'HDC',
  params:[
    {name:'Left stalk position', unit:'', did:0x11},
    {name:'Right stalk position', unit:'', did:0x12},
    {name:'Steering wheel button ID', unit:'', did:0x13},
    {name:'Scroll wheel value', unit:'', did:0x14},
    {name:'Wiper position', unit:'', did:0x15},
  ]
};
ACTUATOR_TESTS['HDC'] = [
  {id:'3901', name:'Button LED Test', desc:'Test COM2000 button backlighting'},
];

ECU_CONFIG_PARAMS['BOITEVIT'] = {
  label:'BOITEVIT',
  params:[
    {name:'Gearbox type', zone:0x2D00, byte:0, mask:0x01, category:'Gearbox Config', type:'ZT_ENUM', enumVals:['0=Manual','1=Automatic AL4','2=Automatic AM6','3=Automatic ZF6HP']},
    {name:'Oil type', zone:0x2D00, byte:0, mask:0x02, category:'Gearbox Config', type:'ZT_ENUM', enumVals:['0=Standard ATF','1=LT 71141','2=ESSO LT 71141','3=Mobil ATF']},
    {name:'Sport mode', zone:0x2D00, byte:0, mask:0x04, category:'Gearbox Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Snow mode', zone:0x2D00, byte:0, mask:0x08, category:'Gearbox Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Steering wheel paddles', zone:0x2D00, byte:0, mask:0x10, category:'Gearbox Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Adaptive shift logic', zone:0x2D00, byte:0, mask:0x20, category:'Gearbox Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['BOITEVIT'] = {
  label:'BOITEVIT',
  params:[
    {name:'Gear Position', unit:'', did:0xA0},
    {name:'Selected Gear', unit:'', did:0xA1},
    {name:'Transmission Oil Temp', unit:'°C', did:0xA2},
    {name:'Engine Torque', unit:'Nm', did:0xA3},
    {name:'Turbine Speed', unit:'rpm', did:0xA4},
    {name:'Output Speed', unit:'rpm', did:0xA5},
    {name:'Current Shift', unit:'', did:0xA6},
    {name:'Line Pressure', unit:'bar', did:0xA7},
  ]
};
ACTUATOR_TESTS['BOITEVIT'] = [
  {id:'3301', name:'Shift Solenoid 1', desc:'Gear selection solenoid 1'},
  {id:'3302', name:'Shift Solenoid 2', desc:'Gear selection solenoid 2'},
  {id:'3303', name:'Lockup Solenoid', desc:'Torque converter lockup solenoid'},
  {id:'3304', name:'Pressure Regulator', desc:'Line pressure regulator'},
];

ECU_CONFIG_PARAMS['SPNEU'] = {
  label:'SPNEU',
  params:[
    {name:'Hydractive suspension present', zone:0x2E00, byte:0, mask:0x01, category:'Suspension Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Default mode', zone:0x2E00, byte:0, mask:0x02, category:'Suspension Config', type:'ZT_ENUM', enumVals:['0=Normal','1=Sport','2=Comfort','3=Automatic']},
    {name:'Default height', zone:0x2E00, byte:0, mask:0x0C, category:'Suspension Config', type:'ZT_ENUM', enumVals:['0=Low (highway)','1=Normal','2=High (rough road)','3=Very high (off-road)']},
    {name:'Sport firmness', zone:0x2E00, byte:0, mask:0x10, category:'Suspension Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Loading compensation', zone:0x2E00, byte:0, mask:0x20, category:'Suspension Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['SPNEU'] = {
  label:'SPNEU',
  params:[
    {name:'Vehicle Height FL', unit:'mm', did:0xB0},
    {name:'Vehicle Height FR', unit:'mm', did:0xB1},
    {name:'Vehicle Height RL', unit:'mm', did:0xB2},
    {name:'Vehicle Height RR', unit:'mm', did:0xB3},
    {name:'Suspension Mode', unit:'', did:0xB4},
    {name:'Current Height State', unit:'', did:0xB5},
    {name:'Pressure Front', unit:'bar', did:0xB6},
    {name:'Pressure Rear', unit:'bar', did:0xB7},
  ]
};
ACTUATOR_TESTS['SPNEU'] = [
  {id:'3401', name:'Front Height Raise', desc:'Raise front suspension'},
  {id:'3402', name:'Front Height Lower', desc:'Lower front suspension'},
  {id:'3403', name:'Rear Height Raise', desc:'Raise rear suspension'},
  {id:'3404', name:'Rear Height Lower', desc:'Lower rear suspension'},
  {id:'3405', name:'Front Stiffen', desc:'Stiffen front dampers'},
  {id:'3406', name:'Rear Stiffen', desc:'Stiffen rear dampers'},
];

ECU_CONFIG_PARAMS['DSG'] = {
  label:'DSG',
  params:[
    {name:'Tyre pressure monitoring', zone:0x3200, byte:0, mask:0x01, category:'TPMS Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Wheel configuration set', zone:0x3200, byte:0, mask:0x02, category:'TPMS Config', type:'ZT_ENUM', enumVals:['0=Standard','1=Summer','2=Winter','3=All-season']},
    {name:'Sensor frequency', zone:0x3200, byte:0, mask:0x04, category:'TPMS Config', type:'ZT_ENUM', enumVals:['0=433 MHz','1=315 MHz']},
    {name:'Low pressure threshold', zone:0x3200, byte:0, mask:0x08, category:'TPMS Config', type:'ZT_NUMERIC', enumVals:null},
    {name:'Learning mode active', zone:0x3200, byte:0, mask:0x10, category:'TPMS Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
  ]
};
ECU_MEAS_PARAMS['DSG'] = {
  label:'DSG',
  params:[
    {name:'Pressure FL', unit:'bar', did:0x21},
    {name:'Pressure FR', unit:'bar', did:0x22},
    {name:'Pressure RL', unit:'bar', did:0x23},
    {name:'Pressure RR', unit:'bar', did:0x24},
    {name:'Temperature FL', unit:'°C', did:0x25},
    {name:'Temperature FR', unit:'°C', did:0x26},
    {name:'Temperature RL', unit:'°C', did:0x27},
    {name:'Temperature RR', unit:'°C', did:0x28},
  ]
};
ACTUATOR_TESTS['DSG'] = [
  {id:'3A01', name:'Sensor Learn (FL)', desc:'Trigger FL sensor learn'},
  {id:'3A02', name:'Sensor Learn (FR)', desc:'Trigger FR sensor learn'},
  {id:'3A03', name:'Sensor Learn (RL)', desc:'Trigger RL sensor learn'},
  {id:'3A04', name:'Sensor Learn (RR)', desc:'Trigger RR sensor learn'},
];

ECU_CONFIG_PARAMS['TELEMAT'] = {
  label:'TELEMAT',
  params:[
    {name:'Navigation system type', zone:0x2000, byte:0, mask:0x01, category:'Telemat Config', type:'ZT_ENUM', enumVals:['0=RT3','1=NAC','2=SMEG','3=RNEG']},
    {name:'Region', zone:0x2000, byte:0, mask:0x02, category:'Telemat Config', type:'ZT_ENUM', enumVals:['0=Europe','1=North America','2=Asia Pacific','3=Middle East']},
    {name:'GPS receiver present', zone:0x2000, byte:0, mask:0x04, category:'Telemat Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Bluetooth module', zone:0x2000, byte:0, mask:0x08, category:'Telemat Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Voice control', zone:0x2000, byte:0, mask:0x10, category:'Telemat Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['TELEMAT'] = {
  label:'TELEMAT',
  params:[
    {name:'GPS satellite count', unit:'', did:0x2001},
    {name:'GPS signal quality', unit:'', did:0x2002},
    {name:'System temperature', unit:'°C', did:0x2003},
    {name:'Battery backup voltage', unit:'V', did:0x2004},
    {name:'Radio tuner status', unit:'', did:0x2005},
  ]
};
ACTUATOR_TESTS['TELEMAT'] = [
];

ECU_CONFIG_PARAMS['AUTORADIO'] = {
  label:'AUTORADIO',
  params:[
    {name:'Vehicle serial number (VIN)', zone:0x2A00, byte:0, mask:0xFF, category:'Radio Config', type:'ZT_STRING', enumVals:null},
    {name:'Usage geographical zone', zone:0x2A00, byte:1, mask:0x07, category:'Radio Config', type:'ZT_ENUM', enumVals:['0=Europe','1=Asia','2=America','3=Africa','4=Oceania']},
    {name:'CD player', zone:0x2A00, byte:1, mask:0x08, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Fader function', zone:0x2A00, byte:1, mask:0x10, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'AM frequency band', zone:0x2A00, byte:1, mask:0x20, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Volume linked to vehicle speed', zone:0x2A00, byte:1, mask:0x40, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Sound amplifier', zone:0x2A00, byte:1, mask:0x80, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Volume level correction law', zone:0x2A00, byte:2, mask:0x07, category:'Radio Config', type:'ZT_ENUM', enumVals:['0=Law N°1','1=Law N°2','2=Law N°3','3=Law N°4']},
    {name:'LO/DX sensitivity curve', zone:0x2A00, byte:2, mask:0x38, category:'Radio Config', type:'ZT_ENUM', enumVals:['0=Curve n°1','1=Curve n°2','2=Curve n°3']},
    {name:'Radiotext function', zone:0x2A00, byte:2, mask:0x40, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'CDtext function', zone:0x2A00, byte:2, mask:0x80, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Parking assistance', zone:0x2A00, byte:3, mask:0x01, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Auxiliary input n°1', zone:0x2A00, byte:3, mask:0x06, category:'Radio Config', type:'ZT_ENUM', enumVals:['0=Missing','1=Classic','2=USB','3=Bluetooth','4=HDMI']},
    {name:'Auxiliary input n°2', zone:0x2A00, byte:3, mask:0x18, category:'Radio Config', type:'ZT_ENUM', enumVals:['0=Missing','1=Classic','2=USB','3=Bluetooth','4=HDMI']},
    {name:'Steering wheel with fixed central controls', zone:0x2A00, byte:3, mask:0x20, category:'Radio Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['AUTORADIO'] = {
  label:'AUTORADIO',
  params:[
    {name:'Radio power state', unit:'', did:0x90},
    {name:'Audio source', unit:'', did:0x91},
    {name:'Volume level', unit:'', did:0x92},
    {name:'Tuner frequency', unit:'MHz', did:0x93},
  ]
};
ACTUATOR_TESTS['AUTORADIO'] = [
];

ECU_CONFIG_PARAMS['AMPLHIFI'] = {
  label:'AMPLHIFI',
  params:[
  ]
};
ECU_MEAS_PARAMS['AMPLHIFI'] = {
  label:'AMPLHIFI',
  params:[
    {name:'Temperature', unit:'°C', did:0x31},
    {name:'Supply Voltage', unit:'V', did:0x32},
    {name:'Amplifier status', unit:'', did:0x33},
    {name:'Input signal present', unit:'', did:0x34},
  ]
};
ACTUATOR_TESTS['AMPLHIFI'] = [
];

ECU_CONFIG_PARAMS['CPL'] = {
  label:'CPL',
  params:[
    {name:'Rain sensor sensitivity', zone:0x3300, byte:0, mask:0x01, category:'Sensor Config', type:'ZT_ENUM', enumVals:['0=Off','1=Low','2=Medium','3=High']},
    {name:'Light sensor sensitivity', zone:0x3300, byte:0, mask:0x02, category:'Sensor Config', type:'ZT_ENUM', enumVals:['0=Off','1=Low','2=Medium','3=High']},
    {name:'Automatic headlights', zone:0x3300, byte:0, mask:0x04, category:'Sensor Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Automatic wipers', zone:0x3300, byte:0, mask:0x08, category:'Sensor Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Coming home delay', zone:0x3300, byte:0, mask:0x10, category:'Sensor Config', type:'ZT_NUMERIC', enumVals:null},
  ]
};
ECU_MEAS_PARAMS['CPL'] = {
  label:'CPL',
  params:[
    {name:'Rain intensity', unit:'', did:0x41},
    {name:'Ambient light level', unit:'lux', did:0x42},
    {name:'Sensor status', unit:'', did:0x43},
    {name:'Temperature', unit:'°C', did:0x44},
  ]
};
ACTUATOR_TESTS['CPL'] = [
  {id:'3B01', name:'Rain Sensor Test', desc:'Trigger rain sensor self-test'},
  {id:'3B02', name:'Light Sensor Test', desc:'Trigger light sensor self-test'},
];

ECU_CONFIG_PARAMS['BML'] = {
  label:'BML',
  params:[
    {name:'Headlight type', zone:0x3400, byte:0, mask:0x01, category:'Lighting Config', type:'ZT_ENUM', enumVals:['0=Halogen','1=Xenon','2=LED']},
    {name:'DRL function', zone:0x3400, byte:0, mask:0x02, category:'Lighting Config', type:'ZT_ENUM', enumVals:['0=Off','1=Daytime running lights','2=Position lights']},
    {name:'Cornering lights', zone:0x3400, byte:0, mask:0x04, category:'Lighting Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Automatic high beam', zone:0x3400, byte:0, mask:0x08, category:'Lighting Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Fog lights with high beam', zone:0x3400, byte:0, mask:0x10, category:'Lighting Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['BML'] = {
  label:'BML',
  params:[
    {name:'Left low beam status', unit:'', did:0x51},
    {name:'Right low beam status', unit:'', did:0x52},
    {name:'Left high beam status', unit:'', did:0x53},
    {name:'Right high beam status', unit:'', did:0x54},
    {name:'DRL status', unit:'', did:0x55},
    {name:'Lighting power mode', unit:'', did:0x56},
  ]
};
ACTUATOR_TESTS['BML'] = [
  {id:'3C01', name:'Low Beam On', desc:'Left low beam activation'},
  {id:'3C02', name:'High Beam On', desc:'Left high beam activation'},
  {id:'3C03', name:'Cornering Light', desc:'Left cornering light activation'},
  {id:'3C04', name:'Fog Light', desc:'Front fog light activation'},
];

ECU_CONFIG_PARAMS['ADC'] = {
  label:'ADC',
  params:[
  ]
};
ECU_MEAS_PARAMS['ADC'] = {
  label:'ADC',
  params:[
    {name:'Transponder status', unit:'', did:0x61},
    {name:'Key programming mode', unit:'', did:0x62},
    {name:'Immobiliser state', unit:'', did:0x63},
    {name:'Registered keys count', unit:'', did:0x64},
  ]
};
ACTUATOR_TESTS['ADC'] = [
  {id:'3D01', name:'Key Learn', desc:'Enter key programming mode'},
  {id:'3D02', name:'Immobiliser Unlock', desc:'Unlock immobiliser for ECU replacement'},
];

ECU_CONFIG_PARAMS['BSM'] = {
  label:'BSM',
  params:[
    {name:'Engine variant', zone:0x3500, byte:0, mask:0x01, category:'BSM Config', type:'ZT_ENUM', enumVals:['0=DW10 (HDi 110)','1=DW12 (HDi 136)','2=ES9 (V6)','3=EW10 (2.0i)','4=EW7 (1.8i)']},
    {name:'Glow plug type', zone:0x3500, byte:0, mask:0x02, category:'BSM Config', type:'ZT_ENUM', enumVals:['0=Not present','1=Standard glow plugs','2=Instant glow plugs']},
    {name:'Engine cooling fan', zone:0x3500, byte:0, mask:0x04, category:'BSM Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Starter relay', zone:0x3500, byte:0, mask:0x08, category:'BSM Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Fuel pump relay', zone:0x3500, byte:0, mask:0x10, category:'BSM Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Dual battery', zone:0x3500, byte:0, mask:0x20, category:'BSM Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Glow plug timer', zone:0x3500, byte:0, mask:0x40, category:'BSM Config', type:'ZT_NUMERIC', enumVals:null},
  ]
};
ECU_MEAS_PARAMS['BSM'] = {
  label:'BSM',
  params:[
    {name:'Supply voltage', unit:'V', did:0x71},
    {name:'Cooling fan speed', unit:'rpm', did:0x72},
    {name:'Engine coolant temp', unit:'°C', did:0x73},
    {name:'Starter relay state', unit:'', did:0x74},
    {name:'Fuel pump relay state', unit:'', did:0x75},
    {name:'BSM internal temp', unit:'°C', did:0x76},
    {name:'Glow plug status', unit:'', did:0x77},
  ]
};
ACTUATOR_TESTS['BSM'] = [
  {id:'3E01', name:'Starter Relay', desc:'Activate starter relay'},
  {id:'3E02', name:'Fuel Pump Relay', desc:'Activate fuel pump relay'},
  {id:'3E03', name:'Cooling Fan Low', desc:'Engine cooling fan low speed'},
  {id:'3E04', name:'Cooling Fan High', desc:'Engine cooling fan high speed'},
  {id:'3E05', name:'Glow Plug Relay', desc:'Activate glow plug relay'},
];

ECU_CONFIG_PARAMS['ALARME'] = {
  label:'ALARME',
  params:[
    {name:'Alarm system enabled', zone:0x3600, byte:0, mask:0x01, category:'Alarm Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Siren type', zone:0x3600, byte:0, mask:0x02, category:'Alarm Config', type:'ZT_ENUM', enumVals:['0=Standard','1=Autonomous (battery backed)','2=Remote']},
    {name:'Trigger zones', zone:0x3600, byte:0, mask:0x04, category:'Alarm Config', type:'ZT_ENUM', enumVals:['0=Perimeter only','1=Perimeter+volumetric','2=Perimeter+volumetric+tilt']},
    {name:'Interior monitoring', zone:0x3600, byte:0, mask:0x08, category:'Alarm Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Tilt sensor', zone:0x3600, byte:0, mask:0x10, category:'Alarm Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Panic alarm', zone:0x3600, byte:0, mask:0x20, category:'Alarm Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['ALARME'] = {
  label:'ALARME',
  params:[
    {name:'Alarm system status', unit:'', did:0x81},
    {name:'Trigger zone last', unit:'', did:0x82},
    {name:'Siren battery', unit:'V', did:0x83},
    {name:'Interior sensor', unit:'', did:0x84},
    {name:'Tilt sensor', unit:'', did:0x85},
  ]
};
ACTUATOR_TESTS['ALARME'] = [
  {id:'3F01', name:'Siren Test', desc:'Trigger alarm siren'},
  {id:'3F02', name:'LED Flash', desc:'Alarm status LED flash'},
];

ECU_CONFIG_PARAMS['MDP_CONDUCT'] = {
  label:'MDP_CONDUCT',
  params:[
    {name:'Window type', zone:0x3700, byte:0, mask:0x01, category:'Driver Door Config', type:'ZT_ENUM', enumVals:['0=Manual','1=Electric','2=One-touch auto']},
    {name:'Mirror type', zone:0x3700, byte:0, mask:0x02, category:'Driver Door Config', type:'ZT_ENUM', enumVals:['0=Manual','1=Electric','2=Electric+fold','3=Electric+fold+memory']},
    {name:'Auto-fold mirrors', zone:0x3700, byte:0, mask:0x04, category:'Driver Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Window closing with remote', zone:0x3700, byte:0, mask:0x08, category:'Driver Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Anti-pinch windows', zone:0x3700, byte:0, mask:0x10, category:'Driver Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['MDP_CONDUCT'] = {
  label:'MDP_CONDUCT',
  params:[
    {name:'Window position', unit:'%', did:0x91},
    {name:'Mirror horizontal', unit:'', did:0x92},
    {name:'Mirror vertical', unit:'', did:0x93},
    {name:'Door lock status', unit:'', did:0x94},
    {name:'Window motor current', unit:'A', did:0x95},
    {name:'Mirror fold status', unit:'', did:0x96},
    {name:'Door handle switch', unit:'', did:0x97},
  ]
};
ACTUATOR_TESTS['MDP_CONDUCT'] = [
  {id:'4001', name:'Window Up', desc:'Driver window upward movement'},
  {id:'4002', name:'Window Down', desc:'Driver window downward movement'},
  {id:'4003', name:'Mirror Fold', desc:'Fold driver mirror'},
  {id:'4004', name:'Mirror Unfold', desc:'Unfold driver mirror'},
  {id:'4005', name:'Lock Actuator', desc:'Driver door lock actuator'},
];

ECU_CONFIG_PARAMS['MDP_PASSAG'] = {
  label:'MDP_PASSAG',
  params:[
    {name:'Window type', zone:0x3701, byte:0, mask:0x01, category:'Pass Door Config', type:'ZT_ENUM', enumVals:['0=Manual','1=Electric','2=One-touch auto']},
    {name:'Mirror type', zone:0x3701, byte:0, mask:0x02, category:'Pass Door Config', type:'ZT_ENUM', enumVals:['0=Manual','1=Electric','2=Electric+fold','3=Electric+fold+memory']},
    {name:'Auto-fold mirrors', zone:0x3701, byte:0, mask:0x04, category:'Pass Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Window closing with remote', zone:0x3701, byte:0, mask:0x08, category:'Pass Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Anti-pinch windows', zone:0x3701, byte:0, mask:0x10, category:'Pass Door Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['MDP_PASSAG'] = {
  label:'MDP_PASSAG',
  params:[
    {name:'Window position', unit:'%', did:0xA1},
    {name:'Mirror horizontal', unit:'', did:0xA2},
    {name:'Mirror vertical', unit:'', did:0xA3},
    {name:'Door lock status', unit:'', did:0xA4},
    {name:'Window motor current', unit:'A', did:0xA5},
    {name:'Mirror fold status', unit:'', did:0xA6},
    {name:'Door handle switch', unit:'', did:0xA7},
  ]
};
ACTUATOR_TESTS['MDP_PASSAG'] = [
  {id:'4101', name:'Window Up', desc:'Passenger window upward movement'},
  {id:'4102', name:'Window Down', desc:'Passenger window downward movement'},
  {id:'4103', name:'Mirror Fold', desc:'Fold passenger mirror'},
  {id:'4104', name:'Mirror Unfold', desc:'Unfold passenger mirror'},
  {id:'4105', name:'Lock Actuator', desc:'Passenger door lock actuator'},
];

ECU_CONFIG_PARAMS['ECRAN_C'] = {
  label:'ECRAN_C',
  params:[
    {name:'Screen variant', zone:0x2500, byte:0, mask:0x01, category:'Screen Config', type:'ZT_ENUM', enumVals:['0=Monochrome','1=Colour CMB','2=Colour RT3','3=Colour RT6']},
    {name:'Brightness', zone:0x2500, byte:0, mask:0x02, category:'Screen Config', type:'ZT_NUMERIC', enumVals:null},
    {name:'Contrast', zone:0x2500, byte:0, mask:0x04, category:'Screen Config', type:'ZT_NUMERIC', enumVals:null},
  ]
};
ECU_MEAS_PARAMS['ECRAN_C'] = {
  label:'ECRAN_C',
  params:[
    {name:'Display state', unit:'', did:0x2501},
    {name:'Backlight level', unit:'', did:0x2502},
    {name:'Internal temperature', unit:'°C', did:0x2503},
  ]
};
ACTUATOR_TESTS['ECRAN_C'] = [
];

ECU_CONFIG_PARAMS['AIDE_STAT'] = {
  label:'AIDE_STAT',
  params:[
    {name:'Parking assist present', zone:0x3800, byte:0, mask:0x01, category:'Park Assist Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Sensor configuration', zone:0x3800, byte:0, mask:0x02, category:'Park Assist Config', type:'ZT_ENUM', enumVals:['0=Rear only','1=Front+Rear','2=Front+Rear+Side']},
    {name:'Audible warnings', zone:0x3800, byte:0, mask:0x04, category:'Park Assist Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Visual display', zone:0x3800, byte:0, mask:0x08, category:'Park Assist Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'CD changer installed', zone:0x3800, byte:0, mask:0x10, category:'Park Assist Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
  ]
};
ECU_MEAS_PARAMS['AIDE_STAT'] = {
  label:'AIDE_STAT',
  params:[
    {name:'Distance rear center', unit:'cm', did:0xB1},
    {name:'Distance rear left', unit:'cm', did:0xB2},
    {name:'Distance rear right', unit:'cm', did:0xB3},
    {name:'Distance front center', unit:'cm', did:0xB4},
    {name:'Distance front left', unit:'cm', did:0xB5},
    {name:'Distance front right', unit:'cm', did:0xB6},
    {name:'CD status', unit:'', did:0xB7},
    {name:'Parking audio tone', unit:'Hz', did:0xB8},
  ]
};
ACTUATOR_TESTS['AIDE_STAT'] = [
  {id:'4201', name:'Rear Buzzer Test', desc:'Activate rear parking buzzer'},
  {id:'4202', name:'Front Buzzer Test', desc:'Activate front parking buzzer'},
  {id:'4203', name:'Sensor Self-Test', desc:'Run parking sensor self-diagnosis'},
];

ECU_CONFIG_PARAMS['PROJECTEURS'] = {
  label:'PROJECTEURS',
  params:[
    {name:'Adaptive headlamps present', zone:0x3900, byte:0, mask:0x01, category:'AFL Config', type:'ZT_BOOL', enumVals:['0=No / Absent','1=Yes / Present']},
    {name:'Lamp type', zone:0x3900, byte:0, mask:0x02, category:'AFL Config', type:'ZT_ENUM', enumVals:['0=Halogen static','1=Xenon directional','2=LED directional']},
    {name:'Beam pattern', zone:0x3900, byte:0, mask:0x04, category:'AFL Config', type:'ZT_ENUM', enumVals:['0=LHD (left traffic)','1=RHD (right traffic)']},
    {name:'Cornering function', zone:0x3900, byte:0, mask:0x08, category:'AFL Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
    {name:'Motorway light', zone:0x3900, byte:0, mask:0x10, category:'AFL Config', type:'ZT_BOOL', enumVals:['0=Deactivated','1=Activated']},
  ]
};
ECU_MEAS_PARAMS['PROJECTEURS'] = {
  label:'PROJECTEURS',
  params:[
    {name:'Left vertical aim', unit:'°', did:0xC1},
    {name:'Right vertical aim', unit:'°', did:0xC2},
    {name:'Left horizontal aim', unit:'°', did:0xC3},
    {name:'Right horizontal aim', unit:'°', did:0xC4},
    {name:'Headlamp level sensor FL', unit:'mm', did:0xC5},
    {name:'Headlamp level sensor RL', unit:'mm', did:0xC6},
  ]
};
ACTUATOR_TESTS['PROJECTEURS'] = [
  {id:'4301', name:'Left Leveling', desc:'Left headlamp vertical leveling'},
  {id:'4302', name:'Right Leveling', desc:'Right headlamp vertical leveling'},
  {id:'4303', name:'Left Cornering', desc:'Left cornering light activation'},
  {id:'4304', name:'Right Cornering', desc:'Right cornering light activation'},
];

/* ---- Open Lexia 3 app logic --------------------------------------------------
 * Drives the Lexia workflow (Global Test -> ECU tree -> per-ECU functions) over
 * the firmware's HTTP contract:
 *   GET /api/cmd?val=<cmd>     send a shell command (fire-and-forget)
 *   GET /api/stream            SSE log stream ("data: <line>")
 *   GET /api/data?type=vehicle|status   initial state sync
 * ECU_CONFIG_PARAMS / ECU_MEAS_PARAMS / ACTUATOR_TESTS come from the generated
 * block above (scripts/gen_ecu_data.py). This code is hand-written.
 * -------------------------------------------------------------------------- */
(function () {
  "use strict";
  var $ = function (id) { return document.getElementById(id); };

  // ECU list mirrors kEcuTable (include/psa/psa_protocol.hpp). id === backend family.
  var ECUS = [
    { id: "INJ",         label: "Engine (INJ)",             note: "EDC16/SID80" },
    { id: "BMF",         label: "BSI (gateway)",            note: "gateway" },
    { id: "ABRASR",      label: "ABS / ESP",                note: "braking" },
    { id: "AIRBAG",      label: "Airbag / SRS",             note: "safety" },
    { id: "CLIM",        label: "Climate control",          note: "comfort" },
    { id: "COMBINE",     label: "Instrument cluster",       note: "dashboard" },
    { id: "DIRECTN",     label: "Power steering",           note: "EPS" },
    { id: "HDC",         label: "Steering column (COM2000)",note: "wheel" },
    { id: "BOITEVIT",    label: "Gearbox",                  note: "auto" },
    { id: "SPNEU",       label: "Suspension",               note: "hydractive" },
    { id: "DSG",         label: "Tyre pressure",            note: "TPMS" },
    { id: "TELEMAT",     label: "Telematics / Nav",         note: "RT3/NAC" },
    { id: "AUTORADIO",   label: "Radio",                    note: "RD4/RD45" },
    { id: "AMPLHIFI",    label: "Amplifier",                note: "hi-fi" },
    { id: "CPL",         label: "Rain / light sensor",      note: "CPL" },
    { id: "BML",         label: "Lighting",                 note: "BML" },
    { id: "ADC",         label: "Immobiliser",              note: "immo" },
    { id: "ALARME",      label: "Alarm",                    note: "ALARMES" },
    { id: "MDP_CONDUCT", label: "Driver door module",       note: "MDPLC_D" },
    { id: "MDP_PASSAG",  label: "Passenger door module",    note: "MDPLC_G" },
    { id: "PROJECTEURS", label: "Directional headlamps",    note: "CORPRO" },
    { id: "ECRAN_C",     label: "Multifunction display",    note: "unverified" },
    { id: "AIDE_STAT",   label: "Parking assistance",       note: "unverified" }
  ];

  var st = {
    connected: false, ecu: null, unlocked: false, scanning: false,
    dtcs: [], meas: { name: "—", unit: "", hist: [] }, activeParams: {},
    identReading: false,
    cfgRaw: {},          // zoneHex -> [byte,...] last read
    cfgPendingZone: null, // zone whose raw hex line we're expecting
    cfgReading: false, cfgQueue: null, cfgTimer: null,
  };

  // ---- theme ----
  function applyTheme(t) {
    document.documentElement.setAttribute("data-theme", t);
    var icon = document.querySelector(".theme-icon");
    if (icon) icon.textContent = t === "light" ? "☀" : "🌙";
    try { localStorage.setItem("c5diag-theme", t); } catch (e) {}
  }
  function initTheme() {
    var saved;
    try { saved = localStorage.getItem("c5diag-theme"); } catch (e) {}
    if (!saved) {
      saved = (window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches) ? "light" : "dark";
    }
    applyTheme(saved);
  }
  function toggleTheme() {
    applyTheme(document.documentElement.getAttribute("data-theme") === "light" ? "dark" : "light");
  }

  // ---- command transport ----
  function cmd(c) {
    logLine("› " + c, "tx");
    return fetch("/api/cmd?val=" + encodeURIComponent(c)).catch(function () {
      logLine("[ui] command not sent (link down?)", "bad");
    });
  }
  function cmdSeq() { // send commands in order
    var list = Array.prototype.slice.call(arguments);
    return list.reduce(function (p, c) { return p.then(function () { return cmd(c); }); }, Promise.resolve());
  }

  // ---- console ----
  function logLine(text, cls) {
    var out = $("consoleOut");
    var d = document.createElement("div");
    d.className = "ln " + (cls || classify(text));
    d.textContent = text;
    out.appendChild(d);
    while (out.childNodes.length > 600) out.removeChild(out.firstChild);
    if ($("chkAutoscroll").checked) out.scrollTop = out.scrollHeight;
  }
  function classify(t) {
    if (/NEGATIVE|timeout|error|failed|stuck/i.test(t)) return "bad";
    if (/succeeded|success|written|cleared|done|Ready|present/i.test(t)) return "ok";
    if (/pending|Warning|No SecurityAccess|not connected/i.test(t)) return "warn";
    if (/^\[DIAG\]|^\[SCAN\]|^\[CONFIG\]|^\[FLASH\]|^\[LIVE\]|^\[GSNIFF\]/.test(t)) return "diag";
    return "sys";
  }

  // ---- ECU tree ----
  function buildTree() {
    var ul = $("ecuTree"); ul.innerHTML = "";
    ECUS.forEach(function (e) {
      var li = document.createElement("li");
      li.dataset.id = e.id;
      li.innerHTML =
        '<span class="st" data-st></span>' +
        '<span class="fam">' + e.id + '</span>' +
        '<span class="note">' + e.label + '</span>' +
        '<span class="badge" data-badge hidden></span>';
      li.addEventListener("click", function () { selectEcu(e.id); });
      ul.appendChild(li);
    });
  }
  function treeRow(id) { return $("ecuTree").querySelector('li[data-id="' + id + '"]'); }
  function setEcuStatus(id, cls, dtcCount) {
    var row = treeRow(id); if (!row) return;
    var dot = row.querySelector("[data-st]");
    dot.className = "st" + (cls ? " " + cls : "");
    var b = row.querySelector("[data-badge]");
    if (dtcCount > 0) { b.hidden = false; b.textContent = dtcCount; } else { b.hidden = true; }
  }
  function markSelected(id) {
    Array.prototype.forEach.call($("ecuTree").children, function (li) {
      li.classList.toggle("sel", li.dataset.id === id);
    });
  }

  // ---- selection / connection ----
  function selectEcu(id) {
    var e = ECUS.filter(function (x) { return x.id === id; })[0];
    markSelected(id);
    $("ecuName").textContent = e.label + "  ·  " + id;
    $("ecuMeta").textContent = "connecting…";
    st.ecu = id; st.unlocked = false; setLock(false);
    resetPanes();
    populateEcuFunctions(id);
    // Restore ECU mode (hide sniff, show sidebar/ECU bar/tabs)
    document.querySelectorAll(".ftab").forEach(function (x) { x.classList.remove("active"); });
    document.querySelectorAll(".pane").forEach(function (x) { x.classList.remove("active"); });
    document.querySelector('.ftab[data-fn="ident"]').classList.add("active");
    document.querySelector('.pane[data-pane="ident"]').classList.add("active");
    document.querySelector('.ecu-bar').style.display = '';
    document.querySelector('.func-tabs').style.display = '';
    document.getElementById('sidebar').style.display = '';
    $("btnSniff").classList.remove("active");
    // exit any prior session, then open this one.
    cmdSeq("exit", "connect " + id);
  }

  function populateEcuFunctions(id) {
    // Measurements dropdown
    var sel = $("measSelect"); sel.innerHTML = "";
    var m = (typeof ECU_MEAS_PARAMS !== "undefined" && ECU_MEAS_PARAMS[id]) || null;
    var params = m ? m.params : [];
    if (!params.length) {
      var o = document.createElement("option"); o.textContent = "No parameters defined"; o.disabled = true; sel.appendChild(o);
    } else {
      params.forEach(function (p) {
        var o = document.createElement("option");
        o.value = hx(p.did); o.textContent = p.name + (p.unit ? " (" + p.unit + ")" : "");
        o.dataset.unit = p.unit || ""; o.dataset.name = p.name;
        sel.appendChild(o);
      });
    }
    // Actuators
    var list = $("actList"); list.innerHTML = "";
    var acts = (typeof ACTUATOR_TESTS !== "undefined" && ACTUATOR_TESTS[id]) || [];
    if (!acts.length) { list.innerHTML = '<li class="muted">No actuator tests defined for this ECU.</li>'; }
    else acts.forEach(function (a) {
      var li = document.createElement("li");
      li.innerHTML = '<span class="act-name">' + esc(a.name) + '</span>' +
        (a.desc ? '<span class="act-desc">' + esc(a.desc) + '</span>' : '') +
        '<button class="btn" data-act="' + hx(a.id) + '">Activate ' + hx(a.id) + '</button>';
      li.querySelector("button").addEventListener("click", function () {
        cmd("actuator " + hx(a.id));
      });
      list.appendChild(li);
    });
    // Telecoding editor
    renderCfgEditor(id);
  }

  function hx(v) { // normalise a hex id ("0x3101"/"3101"/12345) -> bare hex string
    if (typeof v === "number") return v.toString(16).toUpperCase();
    return String(v).replace(/^0x/i, "").toUpperCase();
  }
  function esc(s) { return String(s).replace(/[&<>]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]; }); }

  // ---- lock / connection UI ----
  function setLock(open) {
    st.unlocked = open;
    var l = $("lockState"); l.textContent = open ? "🔓" : "🔒"; l.classList.toggle("open", open);
    $("btnFlashBegin").disabled = !open || !st.connected;
    $("srecInput").disabled = !open;
  }
  function setConnected(on) {
    st.connected = on;
    $("indComms").classList.toggle("on", on);
    var dis = !on;
    ["btnIdent", "btnReadDtc", "btnClearDtc", "btnMeasAdd", "btnMeasStop",
      "btnCfgReadAll", "cfgSearch", "btnUnlock", "measSelect", "btnFlashStatus", "btnFlashCancel",
      "btnFlashEnd", "btnFlashSend"].forEach(function (b) { if ($(b)) $(b).disabled = dis; });
    if (!on) { setLock(false); $("ecuMeta").textContent = "disconnected"; }
  }

  // ---- SSE parsing ----
  function handleLine(t) {
    logLine(t);

    var m;
    // connection
    if ((m = t.match(/Session open with (\w+)\. Ready/))) {
      st.ecu = m[1]; setConnected(true); markSelected(m[1]);
      $("ecuMeta").textContent = "session open";
      if (!st.scanning) $("ecuName").textContent = (labelOf(m[1]) || m[1]) + "  ·  " + m[1];
      return;
    }
    if (/Closing session|Not connected\./.test(t)) { if (!st.scanning) setConnected(false); return; }
    if ((m = t.match(/Connecting to (\w+)/))) { setEcuStatus(m[1], "busy"); return; }

    // security
    if (/Security unlock succeeded/.test(t)) { setLock(true); $("ecuMeta").textContent = "unlocked"; return; }
    if (/No SecurityAccess PIN known/.test(t)) { $("pinInput").classList.add("need"); $("pinInput").focus(); return; }

    // global scan lifecycle
    if (/Starting global ECU scan|PRE-DELIVERY INSPECTION/.test(t)) {
      st.scanning = true; $("indComms").classList.add("busy");
      ECUS.forEach(function (e) { setEcuStatus(e.id, ""); });
      $("scanSummary").textContent = "Global test running…";
      return;
    }
    if ((m = t.match(/^\[SCAN\] (\w+) .*bus/))) { setEcuStatus(m[1], "busy"); return; }
    if ((m = t.match(/^\[SCAN\] (\w+) timeout/))) { setEcuStatus(m[1], "nocomm"); return; }
    // scan report rows:  "  INJ         OK  DTC" | "  CLIM   NO COMM" | "  X  ---"
    if ((m = t.match(/^\s+([A-Z_]+)\s+(OK|NO COMM|---)\b(\s+DTC)?/))) {
      var id = m[1];
      if (ECUS.some(function (e) { return e.id === id; })) {
        if (m[2] === "OK") setEcuStatus(id, m[3] ? "dtc" : "ok", m[3] ? "!" : 0);
        else if (m[2] === "NO COMM") setEcuStatus(id, "nocomm");
        return;
      }
    }
    if ((m = t.match(/Found (\d+) ECUs \((\d+) with DTC\)/))) {
      st.scanning = false; setConnected(false); $("indComms").classList.remove("busy");
      $("scanSummary").textContent = m[1] + " ECUs present · " + m[2] + " with faults";
      return;
    }
    if (/PDI Status: (PASS|FAIL)/.test(t)) { st.scanning = false; $("indComms").classList.remove("busy"); }

    // DTCs
    if (/Reading fault codes from/.test(t)) { st.dtcs = []; renderDtc(); return; }
    if ((m = t.match(/DTC ([0-9A-Fa-f]+) - (\w+) \(status: ([0-9A-Fa-f]+)\)(?:\s*—\s*(\S+):\s*(.*))?/))) {
      st.dtcs.push({ code: m[1].toUpperCase(), state: m[2], status: m[3].toUpperCase(),
                     label: m[4] || "", desc: (m[5] || "").trim() });
      renderDtc();
      if (st.ecu) setEcuStatus(st.ecu, "dtc", st.dtcs.length);
      return;
    }
    if (/Faults cleared successfully/.test(t)) { st.dtcs = []; renderDtc(); if (st.ecu) setEcuStatus(st.ecu, "ok", 0); return; }

    // live measurement
    if ((m = t.match(/^\[LIVE\] (.+): ([-0-9.]+) ?(.*)$/))) { updateMeas(m[1], parseFloat(m[2]), m[3]); return; }

    // config zone read — capture RAW bytes for the telecoding editor
    if ((m = t.match(/^\[CONFIG\] Zone ([0-9A-Fa-f]+):/))) { st.cfgPendingZone = m[1].toUpperCase(); return; }
    if (st.cfgPendingZone && (m = t.match(/^\s*((?:[0-9A-Fa-f]{2}\s+)*[0-9A-Fa-f]{2})\s*$/))) {
      applyCfgRaw(st.cfgPendingZone, m[1]); st.cfgPendingZone = null; return;
    }
    // non-BSI ECUs print raw bytes inline: "[DIAG] Zone 2B00 (4 bytes): AB CD .."
    // capture while reading config, or when re-confirming a zone we've already read.
    if ((m = t.match(/^\[DIAG\] Zone ([0-9A-Fa-f]+) \(\d+ bytes\):\s*((?:[0-9A-Fa-f]{2}\s*)+)/)) &&
        (st.cfgReading || st.cfgRaw[m[1].toUpperCase()])) {
      applyCfgRaw(m[1].toUpperCase(), m[2]); return;
    }

    // identification (VIN via Zone F190/80 ASCII)
    if ((m = t.match(/Zone ([0-9A-Fa-f]+) \(\d+ bytes\):.*ASCII: "(.*)"/))) {
      if (st.identReading) { setIdent(m[1].toUpperCase(), m[2]); }
      if (/F190|^80$/.test(m[1]) && m[2].length >= 11) { $("vehVin").textContent = "VIN " + m[2]; }
      return;
    }
  }

  function labelOf(id) { var e = ECUS.filter(function (x) { return x.id === id; })[0]; return e ? e.label : null; }

  // ---- renderers ----
  function renderDtc() {
    var b = $("dtcBody");
    $("dtcCount").textContent = st.dtcs.length ? st.dtcs.length + " fault(s)" : "";
    if (!st.dtcs.length) { b.innerHTML = '<tr><td class="muted" colspan="3">No faults.</td></tr>'; return; }
    b.innerHTML = st.dtcs.map(function (d) {
      var pill = d.state === "ACTIVE" ? '<span class="pill active">ACTIVE</span>' : '<span class="pill stored">STORED</span>';
      var label = d.label || d.code;
      var desc = d.desc || ("fault " + d.code);
      return '<tr><td class="code">' + label + '</td><td class="muted">' + desc + '</td><td>' + pill + ' <span class="muted">0x' + d.status + '</span></td></tr>';
    }).join("");
  }
  // ---- Telecoding editor (Lexia-style: menu of named settings, no raw hex) ----
  function cfgParamsFor(id) {
    var c = (typeof ECU_CONFIG_PARAMS !== "undefined") && ECU_CONFIG_PARAMS[id];
    return (c && c.params) || [];
  }
  function shiftOf(mask) { var s = 0; if (!mask) return 0; while ((mask & 1) === 0) { mask >>= 1; s++; } return s; }
  function zoneHex(z) { return (z & 0xFFFF).toString(16).toUpperCase().padStart(z > 0xFF ? 4 : 2, "0"); }
  function enumLabel(vals, v) {
    if (!vals) return String(v);
    for (var i = 0; i < vals.length; i++) { var mm = vals[i].match(/^(\d+)=(.*)$/); if (mm && +mm[1] === v) return vals[i]; }
    return String(v);
  }
  // Build the editor DOM for the selected ECU (controls disabled until Read).
  function renderCfgEditor(id) {
    var host = $("cfgEditor"); host.innerHTML = "";
    var params = cfgParamsFor(id);
    if (!params.length) {
      host.innerHTML = '<p class="muted">No telecoding parameters are defined for this ECU.</p>';
      return;
    }
    var groups = {};
    params.forEach(function (p) { (groups[p.category] || (groups[p.category] = [])).push(p); });
    Object.keys(groups).forEach(function (cat) {
      var sec = document.createElement("section"); sec.className = "cfg-group";
      var h = document.createElement("h4"); h.textContent = cat; sec.appendChild(h);
      groups[cat].forEach(function (p, i) {
        var zh = zoneHex(p.zone), sh = shiftOf(p.mask), maxv = p.mask >> sh;
        var row = document.createElement("div"); row.className = "cfg-row";
        row.dataset.zone = zh; row.dataset.byte = p.byte; row.dataset.mask = p.mask; row.dataset.shift = sh;
        row.dataset.filter = (p.name + " " + cat).toLowerCase();
        var ctrl;
        if (p.type === "ZT_NUMERIC" || !p.enumVals) {
          ctrl = document.createElement("input"); ctrl.type = "number"; ctrl.min = 0; ctrl.max = maxv; ctrl.className = "cfg-ctrl";
        } else {
          ctrl = document.createElement("select"); ctrl.className = "cfg-ctrl";
          p.enumVals.forEach(function (v) {
            var mm = v.match(/^(\d+)=(.*)$/); if (!mm) return;
            var o = document.createElement("option"); o.value = mm[1]; o.textContent = v; ctrl.appendChild(o);
          });
        }
        ctrl.disabled = true;
        row.innerHTML =
          '<div class="cfg-info"><span class="cfg-name">' + esc(p.name) + '</span>' +
          '<span class="cfg-zone code">zone ' + zh + ' · byte ' + p.byte + ' · mask 0x' + p.mask.toString(16).toUpperCase() + '</span></div>';
        var right = document.createElement("div"); right.className = "cfg-act";
        var cur = document.createElement("span"); cur.className = "cfg-cur muted"; cur.textContent = "—";
        var apply = document.createElement("button"); apply.className = "btn mini"; apply.textContent = "Apply"; apply.disabled = true;
        apply.addEventListener("click", function () { cfgApply(row, ctrl, p); });
        right.appendChild(cur); right.appendChild(ctrl); right.appendChild(apply);
        row.appendChild(right);
        row._ctrl = ctrl; row._cur = cur; row._apply = apply; row._param = p;
        sec.appendChild(row);
      });
      host.appendChild(sec);
    });
    // if this ECU's zones were already read this session, repaint values
    Object.keys(st.cfgRaw).forEach(function (z) { paintZone(z); });
  }
  // Sequential zone reader — the device answers one request at a time, so we
  // wait for each zone's response (or a watchdog timeout) before the next read.
  function cfgReadAll() {
    var params = cfgParamsFor(st.ecu); if (!params.length) return;
    var zones = []; params.forEach(function (p) { var z = zoneHex(p.zone); if (zones.indexOf(z) < 0) zones.push(z); });
    st.cfgQueue = zones.slice(); st.cfgReading = true;
    $("btnCfgReadAll").disabled = true;
    cfgReadNext();
  }
  function cfgReadNext() {
    clearTimeout(st.cfgTimer);
    if (!st.cfgQueue || !st.cfgQueue.length) { st.cfgReading = false; $("btnCfgReadAll").disabled = !st.connected; return; }
    var z = st.cfgQueue[0];
    cmd("read " + z);
    st.cfgTimer = setTimeout(function () { cfgAdvance(z); }, 2000); // skip a zone that never answered
  }
  function cfgAdvance(z) {
    if (st.cfgQueue && st.cfgQueue[0] === z) { st.cfgQueue.shift(); cfgReadNext(); }
  }
  // Store raw bytes for a zone and refresh every control bound to it.
  function applyCfgRaw(zh, hexStr) {
    var bytes = hexStr.trim().split(/\s+/).map(function (h) { return parseInt(h, 16); }).filter(function (n) { return !isNaN(n); });
    if (!bytes.length) return;
    st.cfgRaw[zh] = bytes;
    paintZone(zh);
    cfgAdvance(zh);
  }
  function paintZone(zh) {
    var raw = st.cfgRaw[zh]; if (!raw) return;
    Array.prototype.forEach.call($("cfgEditor").querySelectorAll('.cfg-row[data-zone="' + zh + '"]'), function (row) {
      var byte = +row.dataset.byte, mask = +row.dataset.mask, sh = +row.dataset.shift;
      if (byte >= raw.length) return;
      var v = (raw[byte] & mask) >> sh;
      row._ctrl.disabled = false; row._apply.disabled = false;
      if (row._ctrl.tagName === "SELECT") row._ctrl.value = String(v); else row._ctrl.value = v;
      row._cur.textContent = "now: " + enumLabel(row._param.enumVals, v);
      row._cur.classList.remove("muted");
    });
  }
  // Read-modify-write: rewrite only the masked bits of one byte, keep the rest.
  function cfgApply(row, ctrl, p) {
    if (!st.unlocked) { alert("Unlock the ECU first (Unlock button)."); return; }
    var zh = row.dataset.zone, byte = +row.dataset.byte, mask = +row.dataset.mask, sh = +row.dataset.shift;
    var raw = st.cfgRaw[zh];
    if (!raw) { alert("Read the configuration first so the other bytes are preserved."); return; }
    var v = parseInt(ctrl.value, 10);
    if (isNaN(v) || v < 0 || v > (mask >> sh)) { alert("Value out of range for this field."); return; }
    var bytes = raw.slice();
    bytes[byte] = (bytes[byte] & ~mask & 0xFF) | ((v << sh) & mask);
    var hex = bytes.map(function (b) { return b.toString(16).toUpperCase().padStart(2, "0"); }).join(" ");
    if (!confirm('Write "' + p.name + '" = ' + enumLabel(p.enumVals, v) + '?\n\nzone ' + zh + ' ← ' + hex)) return;
    cmdSeq("write " + zh + " " + hex, "read " + zh); // write then re-read to confirm
  }
  function cfgFilter(q) {
    q = (q || "").trim().toLowerCase();
    Array.prototype.forEach.call($("cfgEditor").querySelectorAll(".cfg-group"), function (sec) {
      var any = false;
      Array.prototype.forEach.call(sec.querySelectorAll(".cfg-row"), function (row) {
        var hit = !q || row.dataset.filter.indexOf(q) >= 0;
        row.style.display = hit ? "" : "none"; if (hit) any = true;
      });
      sec.style.display = any ? "" : "none";
    });
  }
  function setIdent(zone, ascii) {
    var t = $("identTable").querySelector("tbody");
    if (st.identReading) { t.innerHTML = ""; st.identReading = false; }
    var tr = document.createElement("tr");
    tr.innerHTML = '<td>VIN / Zone ' + zone + '</td><td class="code">' + esc(ascii) + '</td>';
    t.appendChild(tr);
  }

  // ---- measurement sparkline + multi-param table ----
  function updateMeas(name, val, unit) {
    st.meas.name = name; st.meas.unit = unit;
    st.meas.hist.push(val); if (st.meas.hist.length > 120) st.meas.hist.shift();
    $("measName").textContent = name;
    $("measValue").textContent = (Math.round(val * 100) / 100);
    $("measUnit").textContent = unit || "";
    drawSpark();
    // Also update the multi-param table if this param is active
    var rows = st.activeParams;
    for (var key in rows) {
      if (rows[key].name === name) {
        rows[key].value = val;
        rows[key].unit = unit;
        var row = rows[key].row;
        if (row) {
          row.querySelector(".mv").textContent = (Math.round(val * 100) / 100);
          row.querySelector(".mu").textContent = unit || "";
        }
        return;
      }
    }
  }
  function addMeasRow(paramId, paramName) {
    var hex = paramId;
    if (st.activeParams[hex]) return; // already in table
    var tbody = $("measBody");
    var tr = document.createElement("tr");
    tr.innerHTML = '<td class="mn">' + esc(paramName) + '</td>'
      + '<td class="mv">—</td>'
      + '<td class="mu"></td>'
      + '<td><button class="del" data-id="' + hex + '" title="Stop">×</button></td>';
    tbody.appendChild(tr);
    st.activeParams[hex] = { name: paramName, value: NaN, unit: "", row: tr };
    tr.querySelector(".del").addEventListener("click", function () {
      cmd("meas " + hex); // toggle off
      removeMeasRow(hex);
    });
    $("measTableWrap").style.display = "";
    $("measCount").textContent = Object.keys(st.activeParams).length + " active";
  }
  function removeMeasRow(hex) {
    var entry = st.activeParams[hex];
    if (entry && entry.row) { entry.row.remove(); }
    delete st.activeParams[hex];
    var n = Object.keys(st.activeParams).length;
    if (n === 0) {
      $("measTableWrap").style.display = "none";
      $("measCard").classList.remove("hidden");
    }
    $("measCount").textContent = n ? n + " active" : "";
  }
  function clearMeasTable() {
    $("measBody").innerHTML = "";
    st.activeParams = {};
    $("measTableWrap").style.display = "none";
    $("measCard").classList.remove("hidden");
    $("measCount").textContent = "";
  }
  function drawSpark() {
    var cv = $("measSpark"); if (!cv) return;
    var w = cv.clientWidth || 520, h = cv.clientHeight || 90;
    if (cv.width !== w) cv.width = w; if (cv.height !== h) cv.height = h;
    var ctx = cv.getContext("2d"); ctx.clearRect(0, 0, w, h);
    var d = st.meas.hist; if (d.length < 2) return;
    var mn = Math.min.apply(null, d), mx = Math.max.apply(null, d), rng = (mx - mn) || 1;
    var accent = getComputedStyle(document.documentElement).getPropertyValue("--accent").trim() || "#12b3a6";
    ctx.strokeStyle = accent; ctx.lineWidth = 2; ctx.beginPath();
    d.forEach(function (v, i) {
      var x = i / (d.length - 1) * (w - 4) + 2;
      var y = h - 6 - (v - mn) / rng * (h - 12);
      i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    });
    ctx.stroke();
    ctx.strokeStyle = "rgba(18,179,166,.18)"; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(2, h - 6); ctx.lineTo(w - 2, h - 6); ctx.stroke();
  }

  function resetPanes() { st.dtcs = []; renderDtc();
    clearTimeout(st.cfgTimer);
    st.cfgRaw = {}; st.cfgPendingZone = null; st.cfgReading = false; st.cfgQueue = null;
    st.meas.hist = []; $("measValue").textContent = "—"; $("measName").textContent = "—"; $("measUnit").textContent = ""; drawSpark();
    clearMeasTable();
    $("identTable").querySelector("tbody").innerHTML = '<tr><td class="muted" colspan="2">Read the identification.</td></tr>';
  }

  // ---- wiring ----
  function wire() {
    initTheme();
    buildTree();

    // function tabs
    Array.prototype.forEach.call(document.querySelectorAll(".ftab"), function (tab) {
      tab.addEventListener("click", function () {
        document.querySelectorAll(".ftab").forEach(function (x) { x.classList.remove("active"); });
        document.querySelectorAll(".pane").forEach(function (x) { x.classList.remove("active"); });
        tab.classList.add("active");
        document.querySelector('.pane[data-pane="' + tab.dataset.fn + '"]').classList.add("active");
        if (tab.dataset.fn === "meas") drawSpark();
      });
    });

    $("themeToggle").addEventListener("click", function () { toggleTheme(); drawSpark(); });

    $("btnGlobalTest").addEventListener("click", function () {
      st.dtcs = []; renderDtc(); cmdSeq("exit", "scan");
    });
    $("btnSniff").addEventListener("click", function () {
      document.querySelectorAll(".ftab").forEach(function (x) { x.classList.remove("active"); });
      document.querySelectorAll(".pane").forEach(function (x) { x.classList.remove("active"); });
      document.querySelector('.pane[data-pane="sniff"]').classList.add("active");
      document.querySelector('.ecu-bar').style.display = 'none';
      document.querySelector('.func-tabs').style.display = 'none';
      document.getElementById('sidebar').style.display = 'none';
      this.classList.add("active");
    });
    $("btnPdi").addEventListener("click", function () { cmdSeq("exit", "pdi"); });

    $("btnUnlock").addEventListener("click", function () {
      var p = $("pinInput").value.trim();
      $("pinInput").classList.remove("need");
      if (p) cmdSeq("pin " + p, "unlock"); else cmd("unlock");
    });

    $("btnIdent").addEventListener("click", function () {
      st.identReading = true;
      $("identTable").querySelector("tbody").innerHTML = '<tr><td class="muted" colspan="2">Reading…</td></tr>';
      cmd("ident");
    });
    $("btnReadDtc").addEventListener("click", function () { st.dtcs = []; renderDtc(); cmd("dtc"); });
    $("btnClearDtc").addEventListener("click", function () {
      if (confirm("Clear the stored faults of this ECU?")) cmd("clear");
    });

    $("btnMeasAdd").addEventListener("click", function () {
      var o = $("measSelect").selectedOptions[0]; if (!o || !o.value) return;
      var hex = o.value, label = o.textContent || o.text;
      cmd("meas " + hex);
      addMeasRow(hex, label);
    });
    $("btnMeasStop").addEventListener("click", function () {
      cmd("meas off");
      clearMeasTable();
      st.meas.hist = [];
      $("measValue").textContent = "—"; $("measName").textContent = "—"; $("measUnit").textContent = ""; drawSpark();
    });

    $("btnCfgReadAll").addEventListener("click", cfgReadAll);
    $("cfgSearch").addEventListener("input", function () { cfgFilter(this.value); });

    $("btnFlashBegin").addEventListener("click", function () {
      if (confirm("Start the flash sequence (erase + prepare)?")) cmd("flash begin");
    });
    $("btnFlashEnd").addEventListener("click", function () { cmd("flash end"); });
    $("btnFlashStatus").addEventListener("click", function () { cmd("flash status"); });
    $("btnFlashCancel").addEventListener("click", function () { cmd("flash cancel"); });
    $("btnFlashSend").addEventListener("click", function () {
      var lines = $("srecInput").value.split(/\r?\n/).map(function (l) { return l.trim(); }).filter(Boolean);
      lines.reduce(function (p, l) { return p.then(function () { return cmd("flash " + l); }); }, Promise.resolve());
    });

    // sniff monitor — raw CAN bus viewer with HS/LS baud rate selection
    $("btnSniffBack").addEventListener("click", function () {
      document.querySelectorAll(".ftab").forEach(function (x) { x.classList.remove("active"); });
      document.querySelectorAll(".pane").forEach(function (x) { x.classList.remove("active"); });
      document.querySelector('.ftab[data-fn="ident"]').classList.add("active");
      document.querySelector('.pane[data-pane="ident"]').classList.add("active");
      document.querySelector('.ecu-bar').style.display = '';
      document.querySelector('.func-tabs').style.display = '';
      document.getElementById('sidebar').style.display = '';
      $("btnSniff").classList.remove("active");
    });
    Array.prototype.forEach.call(document.querySelectorAll(".sftab"), function (tab) {
      tab.addEventListener("click", function () {
        document.querySelectorAll(".sftab").forEach(function (x) { x.classList.remove("active"); });
        tab.classList.add("active");
        cmd("gsniff rate " + tab.dataset.rate);
      });
    });
    $("btnSniffClear").addEventListener("click", function () { $("consoleOut").innerHTML = ""; });

    // sidebar toggle
    $("sideToggle").addEventListener("click", function () { $("sidebar").classList.toggle("collapsed"); });
    // console
    $("btnClearLog").addEventListener("click", function () { $("consoleOut").innerHTML = ""; });
    $("btnConsoleToggle").addEventListener("click", function () {
      var c = $("console"); c.classList.toggle("min");
      $("btnConsoleToggle").textContent = c.classList.contains("min") ? "▴" : "▾";
    });
    $("consoleInput").addEventListener("keydown", function (e) {
      if (e.key === "Enter" && this.value.trim()) { cmd(this.value.trim()); this.value = ""; }
    });

    connectSSE();
    syncState();
    window.addEventListener("resize", drawSpark);
  }

  function connectSSE() {
    try {
      var es = new EventSource("/api/stream");
      es.onopen = function () { $("indLink").classList.add("on"); logLine("[ui] SSE link established", "sys"); };
      es.onerror = function () { $("indLink").classList.remove("on"); };
      es.onmessage = function (ev) { if (ev.data) handleLine(ev.data); };
    } catch (e) { logLine("[ui] EventSource unavailable", "bad"); }
  }
  function syncState() {
    fetch("/api/data?type=vehicle").then(function (r) { return r.json(); }).then(function (v) {
      if (v.model) $("vehModel").textContent = v.model;
      if (v.vin && v.vin !== "---") $("vehVin").textContent = "VIN " + v.vin;
    }).catch(function () {});
    fetch("/api/data?type=status").then(function (r) { return r.json(); }).then(function (s) {
      if (s.connected) { setConnected(true); if (s.ecu && s.ecu !== "none") { selectEcuSilent(s.ecu); } }
      if (s.unlocked) setLock(true);
    }).catch(function () {});
  }
  function selectEcuSilent(id) {
    markSelected(id); st.ecu = id; populateEcuFunctions(id);
    var e = ECUS.filter(function (x) { return x.id === id; })[0];
    $("ecuName").textContent = (e ? e.label : id) + "  ·  " + id;
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", wire);
  else wire();
})();
