#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- CONFIGURACIÓN DE PINES ---
#define PIN_DHT 2
#define PIN_REED A3 
#define PIN_BUZZER 4
#define PIN_LED_FUEGO 5
#define PIN_LED_INTRUSION 6
#define PIN_IN1 7
#define PIN_IN2 8
#define PIN_BOTON_MENU 9 

#define PIN_LDR A0
#define PIN_POT A1
#define PIN_HUMO A2

// --- CONFIGURACIÓN DE LIBRERÍAS ---
#define TIPO_DHT DHT22 
DHT dht(PIN_DHT, TIPO_DHT);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- MÁQUINA DE ESTADOS (VENTANAS) ---
enum Ventanas { PANTALLA_MONITOREO, PANTALLA_CONFIG, PANTALLA_ALERTA_PUERTA };
Ventanas ventanaActual = PANTALLA_MONITOREO; 

// --- VARIABLES GLOBALES ---
int umbralLuzPorcentaje = 0; 
int luzActualPorcentaje = 0;
bool persianaAbierta = false;
bool ultimoEstadoBoton = HIGH; 

unsigned long ultimoTiempoLCD = 0;
const long intervaloLCD = 2000; 

// --- VARIABLES DE ESTADO DE ALARMAS ---
unsigned long tiempoAperturaPuerta = 0; 
bool cronometroPuertaActivo = false;    
bool alertaPuertaSonando = false; 
bool alertaIncendioSonando = false; 

// Variable para no saturar el monitor serie
unsigned long ultimoTiempoSerial = 0;

void setup() {
  Serial.begin(9600); // Iniciamos el monitor serie
  dht.begin();
  lcd.init();
  lcd.backlight();

  pinMode(PIN_REED, INPUT); 
  pinMode(PIN_BOTON_MENU, INPUT_PULLUP); 
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_POT, INPUT);
  pinMode(PIN_HUMO, INPUT);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_FUEGO, OUTPUT);
  pinMode(PIN_LED_INTRUSION, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  
  lcd.print("Testing Alarma...");
  delay(1000);
  lcd.clear();
}

void loop() {
  leerSensores();
  verificarSeguridad();
  verificarIncendio();
  controlarBuzzerYAlertas(); 
  controlarPersiana();
  imprimirDiagnostico(); // <-- Imprime los valores reales en pantalla

  if (ventanaActual != PANTALLA_ALERTA_PUERTA) {
    revisarBotonMenu();
  }

  switch (ventanaActual) {
    case PANTALLA_MONITOREO:
      mostrarVentanaMonitoreo();
      break;
    case PANTALLA_CONFIG:
      mostrarVentanaConfiguracion();
      break;
    case PANTALLA_ALERTA_PUERTA:
      mostrarVentanaAlertaPuerta();
      break;
  }
}

void verificarSeguridad() {
  int posicionPuerta = analogRead(PIN_REED);
  bool puertaAbierta = (posicionPuerta > 800); 

  if (puertaAbierta) {
    if (!cronometroPuertaActivo) {
      tiempoAperturaPuerta = millis(); 
      // ¡Aquí estaba el error de tipeo! Ya lo borramos.
      cronometroPuertaActivo = true;
      lcd.clear();                     
      ventanaActual = PANTALLA_ALERTA_PUERTA; 
    }
    if (millis() - tiempoAperturaPuerta >= 15000) {
      alertaPuertaSonando = true;
    }
  } 
  else {
    if (cronometroPuertaActivo) {
      cronometroPuertaActivo = false;
      alertaPuertaSonando = false; 
      lcd.clear();
      ventanaActual = PANTALLA_MONITOREO; 
    }
  }
}

void verificarIncendio() {
  int nivelHumo = analogRead(PIN_HUMO);
  // Subimos el umbral a 700 para evitar falsos disparos al iniciar
  if (nivelHumo > 700) { 
    alertaIncendioSonando = true;
  } else {
    alertaIncendioSonando = false;
  }
}

void controlarBuzzerYAlertas() {
  if (alertaIncendioSonando) {
    digitalWrite(PIN_LED_FUEGO, HIGH);
    tone(PIN_BUZZER, 2500); 
  } else {
    digitalWrite(PIN_LED_FUEGO, LOW);
  }

  if (alertaPuertaSonando) {
    digitalWrite(PIN_LED_INTRUSION, HIGH);
    if (!alertaIncendioSonando) { 
      tone(PIN_BUZZER, 1000); 
    }
  } else {
    digitalWrite(PIN_LED_INTRUSION, LOW);
  }

  if (!alertaIncendioSonando && !alertaPuertaSonando) {
    noTone(PIN_BUZZER);
  }
}

// --- 🔎 FUNCIÓN DE DIAGNÓSTICO ---
void imprimirDiagnostico() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoTiempoSerial >= 1000) { // Cada 1 segundo
    ultimoTiempoSerial = tiempoActual;
    
    int valorHumo = analogRead(PIN_HUMO);
    int valorPuerta = analogRead(PIN_REED);
    
    Serial.print("--- DIAGNOSTICO --- | ");
    Serial.print("Valor Humo (A2): "); Serial.print(valorHumo);
    Serial.print(" | Valor Puerta (A3): "); Serial.println(valorPuerta);
    
    if(alertaIncendioSonando) Serial.println("-> [!] ALARMA DE INCENDIO ACTIVA");
    if(alertaPuertaSonando) Serial.println("-> [!] ALARMA DE PUERTA ACTIVA");
    if(!alertaIncendioSonando && !alertaPuertaSonando) Serial.println("-> Todo en orden, buzzer deberia estar apagado.");
  }
}

// Las demás funciones quedan exactamente igual...
void leerSensores() {
  int lecturaLDR = analogRead(PIN_LDR);
  luzActualPorcentaje = map(lecturaLDR, 0, 1023, 0, 100);
  int lecturaPot = analogRead(PIN_POT);
  umbralLuzPorcentaje = map(lecturaPot, 0, 1023, 0, 80); 
}
void revisarBotonMenu() {
  bool estadoBoton = digitalRead(PIN_BOTON_MENU);
  if (estadoBoton == LOW && ultimoEstadoBoton == HIGH) {
    lcd.clear(); 
    if (ventanaActual == PANTALLA_MONITOREO) ventanaActual = PANTALLA_CONFIG;
    else ventanaActual = PANTALLA_MONITOREO;
    delay(200); 
  }
  ultimoEstadoBoton = estadoBoton;
}
void mostrarVentanaMonitoreo() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoTiempoLCD >= intervaloLCD) {
    ultimoTiempoLCD = tiempoActual;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    lcd.setCursor(0, 0); lcd.print("T:"); lcd.print((int)t); lcd.print("C H:"); lcd.print((int)h); lcd.print("%   ");
    lcd.setCursor(0, 1); lcd.print("Luz: "); lcd.print(luzActualPorcentaje); lcd.print("%      ");
  }
}
void mostrarVentanaConfiguracion() {
  lcd.setCursor(0, 0); lcd.print("--- SETTING ---");
  lcd.setCursor(0, 1); lcd.print("Umbral Luz: "); lcd.print(umbralLuzPorcentaje); lcd.print("%   ");
}
void mostrarVentanaAlertaPuerta() {
  lcd.setCursor(0, 0); lcd.print("!ALERTA SISTEMA!");
  lcd.setCursor(0, 1); lcd.print("Puerta Abierta  ");
}
void controlarPersiana() {
  if (luzActualPorcentaje > umbralLuzPorcentaje && !persianaAbierta) { abrirPersiana(); persianaAbierta = true; }
  else if (luzActualPorcentaje < (umbralLuzPorcentaje - 5) && persianaAbierta) { cerrarPersiana(); persianaAbierta = false; }
}
void abrirPersiana() { digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW); delay(2000); digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW); }
void cerrarPersiana() { digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH); delay(2000); digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW); }