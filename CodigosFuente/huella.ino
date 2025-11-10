#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <SdFat.h>
#include <Servo.h>
#include <Hash.h>

// --- Configuración Wi-Fi ---
const char *ssid = "";	   // <<<--- CAMBIA ESTO
const char *password = ""; // <<<--- CAMBIA ESTO

// --- Servidor Web ---
ESP8266WebServer server(80);
String webMessage = "Sistema iniciando..."; // Initial message only
String listMessage = "";
String lastMessage = "Sistema iniciando...";

// --- Configuración Sensor Huella ---
const int FINGERPRINT_RX_PIN = 0; // ESP RX = D3 (GPIO0) <--- Sensor TX
const int FINGERPRINT_TX_PIN = 3; // ESP TX = RX (GPIO3) ---> Sensor RX
SoftwareSerial fingerSerial(FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// --- Configuración Tarjeta SD (Usando SdFat) ---
#define SD_CS_PIN 4 // D2
SdFat sd;
SdFile file;
const char *AUTH_FILENAME = "auth_db.csv";

// --- Estado y Flags ---
// bool enrollmentModeViaWeb = false; // Replaced by the two flags below
bool waitingForFingerToEnroll = false; // True when "Register" clicked, waiting for finger
bool waitingForNameToEnroll = false;   // True after finger captured, waiting for name input
int idToEnroll = -1;

// --- Configuración LEDs ---
const int GREEN_LED_PIN = 2; // D4
const int RED_LED_PIN = 16;	 // D8

// --- Buzzer ---
const int BUZZER_PIN = 15; // D0 en NodeMCU

// --- Configuración Servo ---
const int SERVO_PIN = 5; // D1 (GPIO5)
Servo myServo;
const int SERVO_LOCKED_POS = 0;
const int SERVO_UNLOCKED_POS = 90;
const unsigned long ACTION_DURATION = 2000;

// --- Buffer ---
char msgBuffer[200]; // Kept for potential future use, not actively used by core logic now

// --- Forward declarations ---
void webPrint(const String &msg);
void handleRoot();
void handleCommand();
// String getPage(); // Replaced by direct generation in handleRoot
void signalSuccess(const String &name);
void signalFailure(const String &reason);
void signalErrorPersistent(const String &reason);
void listAuthorizedUsers();
bool enrollAndAuthorizeOnSD(int specificID, const String &name);
bool getFingerprintEnroll(int &newEnrollID);
void verifyAccess();
String getAuthorizedUserName(int idToCheck);
int getNextFreeSensorID();
void borrarIdsSensor();
void playTone(int pin, int frequency, int duration);

// --- HTML ---
// Modified HTML with placeholders for buttons and name form
const char HTML_PROGMEM[] PROGMEM = R"=====(
<!DOCTYPE html><html></html>
)=====";

//====================================================================
// SETUP
//====================================================================
void setup()
{
	Serial.begin(115200);
	while (!Serial)
		;
	delay(100);
	webPrint(F("\n--- Sistema Acceso Huella+SD+Servo (ESP8266) ---"));

	pinMode(GREEN_LED_PIN, OUTPUT);
	pinMode(RED_LED_PIN, OUTPUT);
	pinMode(BUZZER_PIN, OUTPUT); // AÑADIDO: Configurar pin del buzzer como salida

	digitalWrite(GREEN_LED_PIN, LOW);
	digitalWrite(RED_LED_PIN, LOW);
	digitalWrite(BUZZER_PIN, LOW); // AÑADIDO: Asegurarse que el buzzer está apagado al inicio

	myServo.attach(SERVO_PIN);
	myServo.write(SERVO_LOCKED_POS);
	webPrint(F("Servo inicializado en posicion cerrada."));
	delay(500);

	fingerSerial.begin(57600);
	delay(1000);
	webPrint(F("Inicializando sensor huella..."));
	if (finger.verifyPassword())
	{
		webPrint(F("Sensor OK!"));
	}
	else
	{
		webPrint(F("Sensor NO encontrado :("));
		signalErrorPersistent("Fallo Sensor Huella");
	}

	webPrint(F("Inicializando tarjeta SD con SdFat..."));
	if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(4)))
	{
		webPrint(F("************************************"));
		webPrint(F("Fallo inicializacion SdFat!"));
		sd.initErrorHalt(&Serial);
		webPrint(F("************************************"));
		signalErrorPersistent("Fallo Tarjeta SD");
	}
	webPrint(F("Tarjeta SD (SdFat) OK!"));

	webPrint(String("Conectando a ") + ssid);
	WiFi.begin(ssid, password);
	int wifi_retries = 0;
	while (WiFi.status() != WL_CONNECTED && wifi_retries < 30)
	{
		delay(500);
		Serial.print(".");
		wifi_retries++;
	}
	Serial.println();

	if (WiFi.status() == WL_CONNECTED)
	{
		webPrint("Wi-Fi Conectado!");
		webPrint(String("Direccion IP: ") + WiFi.localIP().toString());
		server.on("/", HTTP_GET, handleRoot);
		server.on("/command", HTTP_POST, handleCommand); // Changed endpoint slightly for clarity
		server.begin();
		webPrint("Servidor Web iniciado.");
		webPrint("Abre http://" + WiFi.localIP().toString() + " en tu navegador.");
	}
	else
	{
		webPrint("FALLO conexion Wi-Fi. Reiniciando...");
		delay(1000);
		ESP.restart();
	}
	lastMessage = "Sistema listo. Usa los botones o coloca el dedo."; // Update initial message
	webPrint(lastMessage);
}

//====================================================================
// LOOP PRINCIPAL
//====================================================================
void loop()
{
	server.handleClient(); // Process web requests

	// --- Fingerprint Enrollment Step 1: Capture Fingerprint ---
	if (waitingForFingerToEnroll)
	{
		// Message is already set by handleCommand
		if (getFingerprintEnroll(idToEnroll))
		{
			// Success! Now wait for name
			waitingForFingerToEnroll = false;
			waitingForNameToEnroll = true;
			webPrint(String("Huella capturada para ID #") + idToEnroll + ". Esperando nombre en la web...");
			lastMessage = "Huella capturada OK (ID #" + String(idToEnroll) + "). Por favor, introduce el nombre del usuario y pulsa Guardar.";
		}
		else
		{
			// Failure or timeout during capture
			webPrint("Fallo al capturar huella para registro. Intenta de nuevo desde la web.");
			lastMessage = "Fallo al capturar huella. Pulsa 'Registrar' para intentar de nuevo.";
			waitingForFingerToEnroll = false; // Reset state
			waitingForNameToEnroll = false;
			idToEnroll = -1;
		}
	}
	// --- Normal Operation: Verify Access (only if not enrolling) ---
	else if (!waitingForNameToEnroll)
	{ // Don't verify if waiting for name input
		verifyAccess();
	}
	// --- Fingerprint Enrollment Step 2: Waiting for Name (handled by web request) ---
	// else if (waitingForNameToEnroll) {
	// Do nothing here, waiting for the user to submit the name via the web form
	// The handleCommand function will process it.
	// }

	delay(50); // Small delay for stability
}

//====================================================================
// FUNCIONES WEB Y DE IMPRESIÓN
//====================================================================
void webPrint(const String &msg)
{
	Serial.println(msg);
	// Don't update lastMessage here directly, control it more precisely
}

void handleRoot()
{
	String page = HTML_PROGMEM; // Load template from PROGMEM

	// Determine message class
	String msgClass = "info";
	if (lastMessage.indexOf("Bienvenido") != -1 || lastMessage.indexOf("registrado con exito") != -1 || lastMessage.indexOf("Exito") != -1 || lastMessage.indexOf("OK") != -1 || lastMessage.indexOf("borrado con EXITO") != -1)
	{
		msgClass = "success";
	}
	else if (lastMessage.indexOf("NO AUTORIZADO") != -1 || lastMessage.indexOf("DESCONOCIDA") != -1 || lastMessage.indexOf("Fallo") != -1 || lastMessage.indexOf("Err") != -1 || lastMessage.indexOf("Denegado") != -1)
	{
		msgClass = "error";
	}

	// Generate user list HTML if available
	String userListHtml = "";
	if (listMessage.length() > 0)
	{
		userListHtml = "<div class='userlist'><b>Usuarios Registrados:</b><br>" + listMessage + "</div>";
	}

	// --- Conditional HTML Generation ---
	String actionButtonsHtml = "";
	String nameInputHtml = "";

	if (waitingForNameToEnroll)
	{
		// State: Fingerprint captured, waiting for name input
		nameInputHtml = "<form action='/command' method='POST'>";
		nameInputHtml += "<input type='hidden' name='action' value='submit_name'>";										  // Hidden field for action
		nameInputHtml += "<input type='text' name='username' placeholder='Introduce el nombre aqui' required autofocus>"; // Name input field
		nameInputHtml += "<button type='submit'>Guardar Nombre</button>";
		nameInputHtml += "</form>";
		// Don't show action buttons while waiting for name
		actionButtonsHtml = "";
		// Message should already be set in loop() or handleCommand()
	}
	else if (waitingForFingerToEnroll)
	{
		// State: "Register" clicked, waiting for finger placement
		// Don't show buttons or forms, just the message updated by handleCommand/loop
		actionButtonsHtml = "";
		nameInputHtml = "";
		// Message should already be set by handleCommand
	}
	else
	{
		// State: Idle, show action buttons
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='start_enroll'><button type='submit'>Registrar Nuevo Usuario</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='list'><button type='submit'>Listar Usuarios Registrados</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST' onsubmit='return confirm(\"Estas seguro de que quieres borrar TODOS los usuarios y huellas? Esta accion no se puede deshacer.\");'><input type='hidden' name='action' value='delete'><button type='submit' class='delete'>Borrar TODOS los Usuarios</button></form>";
		nameInputHtml = ""; // No name input in idle state
	}

	// Replace placeholders in the template
	page.replace("{MSG_CLASS}", msgClass);
	page.replace("{LAST_MSG}", lastMessage);
	page.replace("{USER_LIST}", userListHtml);
	page.replace("{ACTION_BUTTONS}", actionButtonsHtml);
	page.replace("{NAME_INPUT_FORM}", nameInputHtml);

	server.send(200, "text/html", page);
	listMessage = ""; // Clear list message after displaying
}

void handleCommand()
{
	if (!server.hasArg("action"))
	{
		webPrint("Comando web invalido (sin accion)");
		lastMessage = "Error: Comando web invalido.";
		server.sendHeader("Location", "/");
		server.send(303);
		return;
	}

	String action = server.arg("action");
	webPrint("Accion Web Recibida: " + action);

	if (action == "start_enroll")
	{
		if (!waitingForFingerToEnroll && !waitingForNameToEnroll)
		{
			waitingForFingerToEnroll = true; // Start waiting for finger
			waitingForNameToEnroll = false;
			idToEnroll = -1;
			lastMessage = "REGISTRO INICIADO. Coloca el dedo en el sensor...";
			webPrint(lastMessage);
		}
		else
		{
			lastMessage = "Registro ya en proceso. Completa el paso actual.";
			webPrint("Intento de iniciar registro mientras otro estaba activo.");
		}
	}
	else if (action == "list")
	{
		listAuthorizedUsers(); // Populates listMessage
		if (listMessage.startsWith("("))
		{ // Check if list function indicated an error or empty
			lastMessage = "Lista de usuarios: " + listMessage;
		}
		else
		{
			lastMessage = "Mostrando lista de usuarios registrados.";
		}
		webPrint("Comando Listar ejecutado.");
		// listMessage will be displayed by handleRoot on refresh
	}
	else if (action == "delete")
	{
		borrarIdsSensor(); // This function now updates lastMessage internally
		webPrint("Comando Borrar ejecutado.");
		// Reset any pending enrollment state
		waitingForFingerToEnroll = false;
		waitingForNameToEnroll = false;
		idToEnroll = -1;
	}
	else if (action == "submit_name")
	{
		if (waitingForNameToEnroll && idToEnroll != -1)
		{
			if (!server.hasArg("username"))
			{
				webPrint("Error: Falta nombre de usuario en la peticion.");
				lastMessage = "Error: No se recibio el nombre.";
				// Stay in waitingForNameToEnroll state
			}
			else
			{
				String userName = server.arg("username");
				userName.trim();
				if (userName.length() > 0 && userName.indexOf(',') == -1)
				{ // Basic validation
					webPrint("Nombre recibido: " + userName + " para ID #" + idToEnroll);
					if (enrollAndAuthorizeOnSD(idToEnroll, userName))
					{
						lastMessage = "Usuario '" + userName + "' (ID #" + String(idToEnroll) + ") registrado con exito!";
						webPrint(lastMessage);
						signalSuccess(userName); // Signal success (LED/Servo)
					}
					else
					{
						lastMessage = "Fallo al guardar usuario '" + userName + "' en SD o sensor.";
						webPrint(lastMessage);
						// Consider deleting the model from sensor if SD write failed?
						// finger.deleteModel(idToEnroll); // Optional cleanup
						signalFailure("Fallo Registro SD");
					}
					// Reset state regardless of SD success/failure, as fingerprint part is done
					waitingForNameToEnroll = false;
					idToEnroll = -1;
				}
				else
				{
					webPrint("Nombre invalido recibido: " + userName);
					lastMessage = "Nombre invalido (no puede estar vacio ni contener comas). Introduce un nombre valido.";
					// Stay in waitingForNameToEnroll state to allow user to retry
				}
			}
		}
		else
		{
			webPrint("Comando 'submit_name' ignorado. No se estaba esperando un nombre.");
			lastMessage = "Error interno: Se recibio un nombre cuando no se esperaba.";
			// Ensure state is reset if this unexpected situation occurs
			waitingForNameToEnroll = false;
			waitingForFingerToEnroll = false;
			idToEnroll = -1;
		}
	}
	else
	{
		webPrint("Accion web desconocida: " + action);
		lastMessage = "Error: Accion desconocida.";
	}

	// Redirect back to the root page to show updated status and interface
	server.sendHeader("Location", "/");
	server.send(303);
}

void handleRoot()
{
	String page = HTML_PROGMEM; // Load template from PROGMEM

	// Determine message class
	String msgClass = "info";
	if (lastMessage.indexOf("Bienvenido") != -1 || lastMessage.indexOf("registrado con exito") != -1 || lastMessage.indexOf("Exito") != -1 || lastMessage.indexOf("OK") != -1 || lastMessage.indexOf("borrado con EXITO") != -1)
	{
		msgClass = "success";
	}
	else if (lastMessage.indexOf("NO AUTORIZADO") != -1 || lastMessage.indexOf("DESCONOCIDA") != -1 || lastMessage.indexOf("Fallo") != -1 || lastMessage.indexOf("Err") != -1 || lastMessage.indexOf("Denegado") != -1)
	{
		msgClass = "error";
	}

	// Generate user list HTML if available
	String userListHtml = "";
	if (listMessage.length() > 0)
	{
		userListHtml = "<div class='userlist'><b>Usuarios Registrados:</b><br>" + listMessage + "</div>";
	}

	// --- Conditional HTML Generation ---
	String actionButtonsHtml = "";
	String nameInputHtml = "";

	if (waitingForNameToEnroll)
	{
		// State: Fingerprint captured, waiting for name input
		nameInputHtml = "<form action='/command' method='POST'>";
		nameInputHtml += "<input type='hidden' name='action' value='submit_name'>";										  // Hidden field for action
		nameInputHtml += "<input type='text' name='username' placeholder='Introduce el nombre aqui' required autofocus>"; // Name input field
		nameInputHtml += "<button type='submit'>Guardar Nombre</button>";
		nameInputHtml += "</form>";
		// Don't show action buttons while waiting for name
		actionButtonsHtml = "";
		// Message should already be set in loop() or handleCommand()
	}
	else if (waitingForFingerToEnroll)
	{
		// State: "Register" clicked, waiting for finger placement
		// Don't show buttons or forms, just the message updated by handleCommand/loop
		actionButtonsHtml = "";
		nameInputHtml = "";
		// Message should already be set by handleCommand
	}
	else
	{
		// State: Idle, show action buttons
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='start_enroll'><button type='submit'>Registrar Nuevo Usuario</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='list'><button type='submit'>Listar Usuarios Registrados</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST' onsubmit='return confirm(\"Estas seguro de que quieres borrar TODOS los usuarios y huellas? Esta accion no se puede deshacer.\");'><input type='hidden' name='action' value='delete'><button type='submit' class='delete'>Borrar TODOS los Usuarios</button></form>";
		nameInputHtml = ""; // No name input in idle state
	}

	// Replace placeholders in the template
	page.replace("{MSG_CLASS}", msgClass);
	page.replace("{LAST_MSG}", lastMessage);
	page.replace("{USER_LIST}", userListHtml);
	page.replace("{ACTION_BUTTONS}", actionButtonsHtml);
	page.replace("{NAME_INPUT_FORM}", nameInputHtml);

	server.send(200, "text/html", page);
	listMessage = ""; // Clear list message after displaying
}

void handleRoot()
{
	String page = HTML_PROGMEM; // Load template from PROGMEM

	// Determine message class
	String msgClass = "info";
	if (lastMessage.indexOf("Bienvenido") != -1 || lastMessage.indexOf("registrado con exito") != -1 || lastMessage.indexOf("Exito") != -1 || lastMessage.indexOf("OK") != -1 || lastMessage.indexOf("borrado con EXITO") != -1)
	{
		msgClass = "success";
	}
	else if (lastMessage.indexOf("NO AUTORIZADO") != -1 || lastMessage.indexOf("DESCONOCIDA") != -1 || lastMessage.indexOf("Fallo") != -1 || lastMessage.indexOf("Err") != -1 || lastMessage.indexOf("Denegado") != -1)
	{
		msgClass = "error";
	}

	// Generate user list HTML if available
	String userListHtml = "";
	if (listMessage.length() > 0)
	{
		userListHtml = "<div class='userlist'><b>Usuarios Registrados:</b><br>" + listMessage + "</div>";
	}

	// --- Conditional HTML Generation ---
	String actionButtonsHtml = "";
	String nameInputHtml = "";

	if (waitingForNameToEnroll)
	{
		// State: Fingerprint captured, waiting for name input
		nameInputHtml = "<form action='/command' method='POST'>";
		nameInputHtml += "<input type='hidden' name='action' value='submit_name'>";										  // Hidden field for action
		nameInputHtml += "<input type='text' name='username' placeholder='Introduce el nombre aqui' required autofocus>"; // Name input field
		nameInputHtml += "<button type='submit'>Guardar Nombre</button>";
		nameInputHtml += "</form>";
		// Don't show action buttons while waiting for name
		actionButtonsHtml = "";
		// Message should already be set in loop() or handleCommand()
	}
	else if (waitingForFingerToEnroll)
	{
		// State: "Register" clicked, waiting for finger placement
		// Don't show buttons or forms, just the message updated by handleCommand/loop
		actionButtonsHtml = "";
		nameInputHtml = "";
		// Message should already be set by handleCommand
	}
	else
	{
		// State: Idle, show action buttons
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='start_enroll'><button type='submit'>Registrar Nuevo Usuario</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='list'><button type='submit'>Listar Usuarios Registrados</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST' onsubmit='return confirm(\"Estas seguro de que quieres borrar TODOS los usuarios y huellas? Esta accion no se puede deshacer.\");'><input type='hidden' name='action' value='delete'><button type='submit' class='delete'>Borrar TODOS los Usuarios</button></form>";
		nameInputHtml = ""; // No name input in idle state
	}

	// Replace placeholders in the template
	page.replace("{MSG_CLASS}", msgClass);
	page.replace("{LAST_MSG}", lastMessage);
	page.replace("{USER_LIST}", userListHtml);
	page.replace("{ACTION_BUTTONS}", actionButtonsHtml);
	page.replace("{NAME_INPUT_FORM}", nameInputHtml);

	server.send(200, "text/html", page);
	listMessage = ""; // Clear list message after displaying
}

void handleRoot()
{
	String page = HTML_PROGMEM; // Load template from PROGMEM

	// Determine message class
	String msgClass = "info";
	if (lastMessage.indexOf("Bienvenido") != -1 || lastMessage.indexOf("registrado con exito") != -1 || lastMessage.indexOf("Exito") != -1 || lastMessage.indexOf("OK") != -1 || lastMessage.indexOf("borrado con EXITO") != -1)
	{
		msgClass = "success";
	}
	else if (lastMessage.indexOf("NO AUTORIZADO") != -1 || lastMessage.indexOf("DESCONOCIDA") != -1 || lastMessage.indexOf("Fallo") != -1 || lastMessage.indexOf("Err") != -1 || lastMessage.indexOf("Denegado") != -1)
	{
		msgClass = "error";
	}

	// Generate user list HTML if available
	String userListHtml = "";
	if (listMessage.length() > 0)
	{
		userListHtml = "<div class='userlist'><b>Usuarios Registrados:</b><br>" + listMessage + "</div>";
	}

	// --- Conditional HTML Generation ---
	String actionButtonsHtml = "";
	String nameInputHtml = "";

	if (waitingForNameToEnroll)
	{
		// State: Fingerprint captured, waiting for name input
		nameInputHtml = "<form action='/command' method='POST'>";
		nameInputHtml += "<input type='hidden' name='action' value='submit_name'>";										  // Hidden field for action
		nameInputHtml += "<input type='text' name='username' placeholder='Introduce el nombre aqui' required autofocus>"; // Name input field
		nameInputHtml += "<button type='submit'>Guardar Nombre</button>";
		nameInputHtml += "</form>";
		// Don't show action buttons while waiting for name
		actionButtonsHtml = "";
		// Message should already be set in loop() or handleCommand()
	}
	else if (waitingForFingerToEnroll)
	{
		// State: "Register" clicked, waiting for finger placement
		// Don't show buttons or forms, just the message updated by handleCommand/loop
		actionButtonsHtml = "";
		nameInputHtml = "";
		// Message should already be set by handleCommand
	}
	else
	{
		// State: Idle, show action buttons
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='start_enroll'><button type='submit'>Registrar Nuevo Usuario</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='list'><button type='submit'>Listar Usuarios Registrados</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST' onsubmit='return confirm(\"Estas seguro de que quieres borrar TODOS los usuarios y huellas? Esta accion no se puede deshacer.\");'><input type='hidden' name='action' value='delete'><button type='submit' class='delete'>Borrar TODOS los Usuarios</button></form>";
		nameInputHtml = ""; // No name input in idle state
	}

	// Replace placeholders in the template
	page.replace("{MSG_CLASS}", msgClass);
	page.replace("{LAST_MSG}", lastMessage);
	page.replace("{USER_LIST}", userListHtml);
	page.replace("{ACTION_BUTTONS}", actionButtonsHtml);
	page.replace("{NAME_INPUT_FORM}", nameInputHtml);

	server.send(200, "text/html", page);
	listMessage = ""; // Clear list message after displaying
}

void handleRoot()
{
	String page = HTML_PROGMEM; // Load template from PROGMEM

	// Determine message class
	String msgClass = "info";
	if (lastMessage.indexOf("Bienvenido") != -1 || lastMessage.indexOf("registrado con exito") != -1 || lastMessage.indexOf("Exito") != -1 || lastMessage.indexOf("OK") != -1 || lastMessage.indexOf("borrado con EXITO") != -1)
	{
		msgClass = "success";
	}
	else if (lastMessage.indexOf("NO AUTORIZADO") != -1 || lastMessage.indexOf("DESCONOCIDA") != -1 || lastMessage.indexOf("Fallo") != -1 || lastMessage.indexOf("Err") != -1 || lastMessage.indexOf("Denegado") != -1)
	{
		msgClass = "error";
	}

	// Generate user list HTML if available
	String userListHtml = "";
	if (listMessage.length() > 0)
	{
		userListHtml = "<div class='userlist'><b>Usuarios Registrados:</b><br>" + listMessage + "</div>";
	}

	// --- Conditional HTML Generation ---
	String actionButtonsHtml = "";
	String nameInputHtml = "";

	if (waitingForNameToEnroll)
	{
		// State: Fingerprint captured, waiting for name input
		nameInputHtml = "<form action='/command' method='POST'>";
		nameInputHtml += "<input type='hidden' name='action' value='submit_name'>";										  // Hidden field for action
		nameInputHtml += "<input type='text' name='username' placeholder='Introduce el nombre aqui' required autofocus>"; // Name input field
		nameInputHtml += "<button type='submit'>Guardar Nombre</button>";
		nameInputHtml += "</form>";
		// Don't show action buttons while waiting for name
		actionButtonsHtml = "";
		// Message should already be set in loop() or handleCommand()
	}
	else if (waitingForFingerToEnroll)
	{
		// State: "Register" clicked, waiting for finger placement
		// Don't show buttons or forms, just the message updated by handleCommand/loop
		actionButtonsHtml = "";
		nameInputHtml = "";
		// Message should already be set by handleCommand
	}
	else
	{
		// State: Idle, show action buttons
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='start_enroll'><button type='submit'>Registrar Nuevo Usuario</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST'><input type='hidden' name='action' value='list'><button type='submit'>Listar Usuarios Registrados</button></form>";
		actionButtonsHtml += "<form action='/command' method='POST' onsubmit='return confirm(\"Estas seguro de que quieres borrar TODOS los usuarios y huellas? Esta accion no se puede deshacer.\");'><input type='hidden' name='action' value='delete'><button type='submit' class='delete'>Borrar TODOS los Usuarios</button></form>";
		nameInputHtml = ""; // No name input in idle state
	}

	// Replace placeholders in the template
	page.replace("{MSG_CLASS}", msgClass);
	page.replace("{LAST_MSG}", lastMessage);
	page.replace("{USER_LIST}", userListHtml);
	page.replace("{ACTION_BUTTONS}", actionButtonsHtml);
	page.replace("{NAME_INPUT_FORM}", nameInputHtml);

	server.send(200, "text/html", page);
	listMessage = ""; // Clear list message after displaying
}

//====================================================================
// FUNCIONES DE SEÑALIZACIÓN (LEDs Y SERVO) - No changes needed
//====================================================================

void signalSuccess(const String &name)
{
	digitalWrite(RED_LED_PIN, LOW);
	digitalWrite(GREEN_LED_PIN, HIGH);
	playTone(BUZZER_PIN, 3500, 200); // Tono de 1kHz por 500ms
	delay(100);
	playTone(BUZZER_PIN, 3500, 200); // Tono de 1kHz por 500ms

	myServo.write(SERVO_UNLOCKED_POS);
	webPrint("Accion: Exito para " + name); // Log action
	delay(ACTION_DURATION);
	digitalWrite(GREEN_LED_PIN, LOW);
	myServo.write(SERVO_LOCKED_POS);
	delay(500);
}

void signalFailure(const String &reason)
{
	digitalWrite(GREEN_LED_PIN, LOW);
	digitalWrite(RED_LED_PIN, HIGH);
	playTone(BUZZER_PIN, 700, 500); // Tono de 1kHz por 500ms

	webPrint("Accion: Fallo - " + reason); // Log action
	myServo.write(SERVO_LOCKED_POS);	   // Ensure locked
	delay(ACTION_DURATION);
	digitalWrite(RED_LED_PIN, LOW);
}

void signalErrorPersistent(const String &reason)
{
	Serial.println("!! ERROR PERSISTENTE: " + reason + " !! Deteniendo.");
	lastMessage = "ERROR CRITICO: " + reason + ". Reinicio necesario."; // Update web message if possible
	digitalWrite(GREEN_LED_PIN, LOW);
	for (int i = 0; i < 10; i++)
	{ // Blink RED fast
		digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN));
		delay(100);
	}
	digitalWrite(RED_LED_PIN, HIGH); // Leave RED on
	while (1)
	{
		delay(100);
	} // Halt execution
}

//====================================================================
// FUNCIONES DE HUELLA Y SD - Minor changes/logging additions
//====================================================================

int getNextFreeSensorID()
{
	Serial.println("Buscando ID libre en sensor..."); // Debug log
	for (int id = 1; id < 128; id++)
	{ // Standard range for many sensors
		uint8_t p = finger.loadModel(id);
		if (p == FINGERPRINT_OK)
		{
			// Serial.print(id); Serial.println(" Ocupado"); // Verbose log
			continue; // ID exists, try next
		}
		else if (p == FINGERPRINT_PACKETRECIEVEERR)
		{
			Serial.print("ID libre encontrado: ");
			Serial.println(id);
			return id; // Found an empty slot
		}
		else if (p == FINGERPRINT_BADLOCATION)
		{
			Serial.println("Error: ID fuera de rango durante busqueda.");
			return -1; // Sensor reported invalid ID range
		}
		else
		{
			// Other error, but might indicate the slot is usable or sensor issue
			Serial.print("Error desconocido (0x");
			Serial.print(p, HEX);
			Serial.print(") buscando ID ");
			Serial.print(id);
			Serial.println(". Asumiendo libre.");
			return id; // Assume it's free/usable on error other than packet error
		}
		delay(10); // Small delay between checks
	}
	Serial.println("No se encontraron IDs libres en el sensor (1-127).");
	return -1; // No free ID found
}

// Modified getFingerprintEnroll to integrate server.handleClient() and use webPrint less
bool getFingerprintEnroll(int &newEnrollID)
{
	newEnrollID = getNextFreeSensorID();
	if (newEnrollID == -1)
	{
		lastMessage = "Error: No hay IDs libres en el sensor.";
		webPrint(lastMessage);
		return false;
	}

	webPrint(String("Intentando registro en sensor para ID #") + newEnrollID);
	lastMessage = "Coloca el dedo en el sensor..."; // Update web message

	// --- Paso 1: Capturar primera imagen ---
	Serial.println("Esperando dedo para imagen 1...");
	int p = -1;
	unsigned long startEnroll = millis();
	while (p != FINGERPRINT_OK)
	{
		p = finger.getImage();
		switch (p)
		{
		case FINGERPRINT_OK:
			Serial.println("Imagen 1 tomada.");
			break;
		case FINGERPRINT_NOFINGER:
			server.handleClient(); // Keep web server responsive
			if (millis() - startEnroll > 10000)
			{ // 10 second timeout
				lastMessage = "Timeout esperando dedo (Imagen 1).";
				webPrint(lastMessage);
				return false;
			}
			delay(50);
			continue; // Continue waiting
		default:
			lastMessage = "Error capturando imagen 1.";
			webPrint(lastMessage);
			return false;
		}
	}

	p = finger.image2Tz(1);
	if (p != FINGERPRINT_OK)
	{
		lastMessage = "Error convirtiendo imagen 1.";
		webPrint(lastMessage);
		return false;
	}
	Serial.println("Imagen 1 convertida.");
	lastMessage = "¡Quita el dedo del sensor!"; // Update web message

	// --- Paso 2: Esperar a que quite el dedo ---
	Serial.println("Esperando que quite el dedo...");
	tone(BUZZER_PIN, 2800, 200);
	delay(1000); // Give some time before checking
	startEnroll = millis();
	p = FINGERPRINT_OK; // Assume finger is still there initially
	while (p != FINGERPRINT_NOFINGER)
	{
		p = finger.getImage();
		server.handleClient(); // Keep web server responsive
		if (millis() - startEnroll > 5000)
		{ // 5 second timeout
			lastMessage = "Timeout esperando que quite el dedo.";
			webPrint(lastMessage);
			return false;
		}
		delay(50);
	}
	Serial.println("Dedo quitado.");
	lastMessage = "Coloca el MISMO dedo otra vez..."; // Update web message

	// --- Paso 3: Capturar segunda imagen ---
	Serial.println("Esperando dedo para imagen 2...");
	startEnroll = millis();
	p = -1;
	while (p != FINGERPRINT_OK)
	{
		p = finger.getImage();
		switch (p)
		{
		case FINGERPRINT_OK:
			Serial.println("Imagen 2 tomada.");
			break;
		case FINGERPRINT_NOFINGER:
			server.handleClient(); // Keep web server responsive
			if (millis() - startEnroll > 10000)
			{ // 10 second timeout
				lastMessage = "Timeout esperando dedo (Imagen 2).";
				webPrint(lastMessage);
				return false;
			}
			delay(50);
			continue; // Continue waiting
		default:
			lastMessage = "Error capturando imagen 2.";
			webPrint(lastMessage);
			return false;
		}
	}

	p = finger.image2Tz(2);
	if (p != FINGERPRINT_OK)
	{
		lastMessage = "Error convirtiendo imagen 2.";
		webPrint(lastMessage);
		return false;
	}
	Serial.println("Imagen 2 convertida.");

	// --- Paso 4: Crear y guardar modelo ---
	Serial.print("Creando modelo...");
	p = finger.createModel();
	if (p != FINGERPRINT_OK)
	{
		if (p == FINGERPRINT_ENROLLMISMATCH)
		{
			lastMessage = "Error: Las huellas no coinciden.";
		}
		else
		{
			lastMessage = "Error creando modelo.";
		}
		webPrint(lastMessage);
		playTone(BUZZER_PIN, 700, 500);
		return false;
	}
	Serial.println(" Modelo creado OK.");
	// Model is created, but NOT stored in the sensor's flash memory yet.
	// It's held in the sensor's RAM (CharBuffer1/2).
	// We will store it using storeModel() only *after* getting the name.
	digitalWrite(3, HIGH);
	playTone(BUZZER_PIN, 4000, 200);
	delay(200);
	playTone(BUZZER_PIN, 3500, 200);
	delay(200);
	playTone(BUZZER_PIN, 4000, 200);
	delay(200);
	playTone(BUZZER_PIN, 5000, 500);
	delay(100);

	webPrint("Modelo de huella capturado temporalmente. Esperando nombre...");
	// Don't update lastMessage here, loop() or handleCommand() will do it.
	return true; // Success, ready to store after getting name
}

bool enrollAndAuthorizeOnSD(int specificID, const String &name)
{
	webPrint(String("Guardando modelo final en sensor (ID #") + specificID + ")... ");
	int p = finger.storeModel(specificID); // Store the model created in getFingerprintEnroll
	if (p == FINGERPRINT_OK)
	{
		webPrint("-> Modelo guardado en sensor OK.");
	}
	else
	{
		webPrint(String("-> Error guardando modelo en sensor: 0x") + String(p, HEX));
		return false; // Don't proceed to SD if sensor save failed
	}

	webPrint("Guardando autorizacion en SD (" + String(AUTH_FILENAME) + ")...");
	if (!file.open(AUTH_FILENAME, O_WRITE | O_CREAT | O_APPEND))
	{
		webPrint("-> Error abriendo archivo " + String(AUTH_FILENAME) + " para escritura.");
		// Attempt to delete the model from sensor since SD failed?
		Serial.println("Intentando borrar modelo del sensor debido a fallo de SD...");
		if (finger.deleteModel(specificID) == FINGERPRINT_OK)
		{
			Serial.println("Modelo borrado del sensor.");
		}
		else
		{
			Serial.println("Fallo al borrar modelo del sensor despues de error SD.");
		}
		return false;
	}
	// Write ID,Name format
	file.print(specificID);
	file.print(",");
	file.println(name);
	file.close(); // Close file to save changes
	if (!file.isOpen())
	{ // Check if close was successful (optional)
		webPrint("-> Autorizacion guardada en SD OK.");
		return true;
	}
	else
	{
		webPrint("-> Error: El archivo SD no se cerro correctamente despues de escribir.");
		// SD write might be corrupted. Attempt cleanup.
		Serial.println("Intentando borrar modelo del sensor debido a fallo de cierre de SD...");
		if (finger.deleteModel(specificID) == FINGERPRINT_OK)
		{
			Serial.println("Modelo borrado del sensor.");
		}
		else
		{
			Serial.println("Fallo al borrar modelo del sensor despues de error cierre SD.");
		}
		return false;
	}
}

// No changes needed for getAuthorizedUserName
String getAuthorizedUserName(int idToCheck)
{
	String authorizedName = "";
	if (!file.open(AUTH_FILENAME, O_READ))
	{
		if (sd.exists(AUTH_FILENAME))
		{
			webPrint("Advertencia: No se pudo abrir el archivo de autorizacion para lectura.");
		}
		else
		{
			// File doesn't exist, perfectly normal if no users registered yet
			// webPrint("Archivo de autorización no encontrado (vacio).");
		}
		return authorizedName; // Return empty string
	}

	char lineBuffer[100];
	file.seekSet(0); // Ensure reading from the beginning
	while (file.fgets(lineBuffer, sizeof(lineBuffer)) > 0)
	{
		// Remove trailing newline/carriage return
		lineBuffer[strcspn(lineBuffer, "\r\n")] = 0;

		if (strlen(lineBuffer) > 2 && strchr(lineBuffer, ','))
		{ // Basic check for "ID,Name" format
			char *idStr = strtok(lineBuffer, ",");
			char *nameStr = strtok(NULL, "\n"); // Read rest of line as name

			if (idStr != NULL && nameStr != NULL)
			{
				if (atoi(idStr) == idToCheck)
				{
					authorizedName = String(nameStr);
					break; // Found the user, exit loop
				}
			}
		}
		// strtok modifies the buffer, but we read line by line, so it's okay here.
	}
	file.close();
	return authorizedName;
}

// No major changes needed for verifyAccess, but adjust messages
void verifyAccess()
{
	const int MAX_ATTEMPTS = 3;
	const int RETRY_DELAY_MS = 50;
	bool fingerPresent = false;
	bool accessGranted = false;
	bool errorOccurred = false;
	String userName = "";
	int foundID = -1;
	int confidence = 0;

	uint8_t p = finger.getImage();
	if (p == FINGERPRINT_NOFINGER)
	{
		return; // Idle, no finger
	}
	if (p != FINGERPRINT_OK)
	{
		// webPrint("Verify: Err inicial img check"); // Avoid spamming console
		delay(200); // Small delay if there's a read error
		return;
	}

	fingerPresent = true;
	webPrint("Verificando dedo...");

	p = finger.image2Tz(1);
	if (p != FINGERPRINT_OK)
	{
		webPrint("Verify: Err conv");
		errorOccurred = true;
	}
	else
	{
		// Try searching a few times quickly
		for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
		{
			p = finger.fingerSearch();
			if (p == FINGERPRINT_OK)
			{
				foundID = finger.fingerID;
				confidence = finger.confidence;
				webPrint(String("Huella reconocida (ID #") + foundID + ", Confianza: " + confidence + ")");
				userName = getAuthorizedUserName(foundID);
				if (userName.length() > 0)
				{
					accessGranted = true;
				}
				else
				{
					webPrint(String("ID #") + foundID + " encontrado en sensor, pero NO AUTORIZADO en SD.");
					userName = "ID " + String(foundID) + " (No en SD)";
					accessGranted = false;
				}
				break; // Exit loop on match (authorized or not)
			}
			else if (p == FINGERPRINT_NOTFOUND)
			{
				if (attempt == MAX_ATTEMPTS)
				{
					webPrint("Huella DESCONOCIDA tras " + String(MAX_ATTEMPTS) + " intentos.");
				}
				delay(RETRY_DELAY_MS); // Wait briefly before next search attempt
			}
			else
			{
				webPrint("Verify: Err sensor search");
				errorOccurred = true;
				break; // Exit loop on search error
			}
		}
	}

	// --- Handle Results ---
	if (accessGranted)
	{
		lastMessage = "Bienvenido, " + userName + "! (Conf: " + String(confidence) + ")";
		webPrint(lastMessage);
		signalSuccess(userName);
	}
	else if (errorOccurred)
	{
		lastMessage = "Error durante la lectura de huella.";
		webPrint(lastMessage);
		signalFailure("Error Lectura");
	}
	else if (fingerPresent)
	{ // Finger present, but no match or not authorized
		if (userName.length() > 0 && !accessGranted)
		{ // Known ID in sensor, not authorized on SD
			lastMessage = "Acceso Denegado: Usuario " + userName + " no autorizado.";
		}
		else
		{ // Truly unknown finger
			lastMessage = "Acceso Denegado: Huella desconocida.";
		}
		webPrint(lastMessage);
		signalFailure("Acceso Denegado");
	}

	// --- Prompt to remove finger by waiting until NOFINGER is detected ---
	if (fingerPresent)
	{
		// webPrint("Esperando que quite el dedo post-verificacion...");
		unsigned long startWait = millis();
		while (finger.getImage() != FINGERPRINT_NOFINGER && (millis() - startWait < 3000))
		{
			delay(100);
		}
		// webPrint("Dedo quitado post-verificacion.");
	}
}

// Updated list function to ensure it provides feedback via listMessage
void listAuthorizedUsers()
{
	listMessage = ""; // Clear previous list
	if (!sd.exists(AUTH_FILENAME))
	{
		listMessage = "(No hay usuarios registrados)";
		webPrint(listMessage);
		return;
	}
	if (!file.open(AUTH_FILENAME, O_READ))
	{
		listMessage = "(Error al abrir archivo para listar)";
		webPrint(listMessage);
		return;
	}

	bool anyUsers = false;
	char lineBuffer[100];
	listMessage += "<b>ID, Nombre</b><br>"; // Header for the list
	file.seekSet(0);						// Start from beginning
	while (file.fgets(lineBuffer, sizeof(lineBuffer)) > 0)
	{
		lineBuffer[strcspn(lineBuffer, "\r\n")] = 0; // Remove newline
		if (strlen(lineBuffer) > 2 && strchr(lineBuffer, ','))
		{
			// Simple validation passed, add to list
			listMessage += String(lineBuffer) + "<br>";
			anyUsers = true;
		}
	}
	file.close();

	if (!anyUsers)
	{
		listMessage = "(Lista vacia o formato incorrecto en archivo)";
	}
	// webPrint("Lista de usuarios generada para la web."); // Log success
	// listMessage is now populated for handleRoot
}

// Updated delete function to provide feedback via lastMessage
void borrarIdsSensor()
{
	webPrint("--- INICIANDO BORRADO TOTAL ---");

	// --- 1. Borrar Base de Datos del Sensor ---
	webPrint("Borrando base de datos del sensor...");
	uint8_t p = finger.emptyDatabase();
	bool sensorCleared = false;
	if (p == FINGERPRINT_OK)
	{
		webPrint("-> ¡Base de datos del sensor borrada con EXITO!");
		sensorCleared = true;
	}
	else
	{
		webPrint(String("-> ERROR borrando base de datos del sensor: 0x") + String(p, HEX));
		// Decide whether to proceed if sensor clear fails (maybe SD should still be cleared?)
	}

	// --- 2. Borrar Archivo SD ---
	webPrint("Borrando archivo de autorizacion (" + String(AUTH_FILENAME) + ") de la SD...");
	bool sdCleared = false;
	if (sd.exists(AUTH_FILENAME))
	{
		if (sd.remove(AUTH_FILENAME))
		{
			webPrint("-> ¡Archivo " + String(AUTH_FILENAME) + " borrado de la SD con EXITO!");
			sdCleared = true;
		}
		else
		{
			webPrint("-> ERROR al borrar el archivo " + String(AUTH_FILENAME) + " de la SD.");
			// sd.errorPrint(&Serial); // Optional: Print detailed SdFat error
		}
	}
	else
	{
		webPrint("-> Archivo " + String(AUTH_FILENAME) + " no encontrado en la SD (ya estaba borrado).");
		sdCleared = true; // Consider it cleared if it wasn't there
	}

	// --- 3. Actualizar estado y finalizar ---
	if (sensorCleared && sdCleared)
	{
		lastMessage = "EXITO! Todos los usuarios y huellas han sido borrados.";
	}
	else
	{
		lastMessage = "ATENCION: Ocurrieron errores durante el borrado. ";
		if (!sensorCleared)
			lastMessage += "Fallo sensor. ";
		if (!sdCleared)
			lastMessage += "Fallo SD.";
	}
	webPrint(lastMessage); // Log final status
	webPrint("--- BORRADO TOTAL COMPLETADO ---");

	// Clear any potential leftover state variables
	waitingForFingerToEnroll = false;
	waitingForNameToEnroll = false;
	idToEnroll = -1;
	listMessage = ""; // Clear user list displayed on web
}

void playTone(int pin, int frequency, int duration)
{
	tone(pin, frequency);
	delay(duration);
	noTone(pin);
}