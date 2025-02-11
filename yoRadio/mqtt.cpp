#include "mqtt.h"

#ifdef MQTT_HOST
#include "WiFi.h"

#include "telnet.h"
#include "player.h"
#include "config.h"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

void connectToMqtt() {
  mqttClient.connect();
}

void mqttInit() {
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  if(MQTT_USER!="") mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  connectToMqtt();
}

void onMqttConnect(bool sessionPresent) {
  char buf[140];
  sprintf(buf, "%s%s", MQTT_ROOT_TOPIC, "command");
  mqttClient.subscribe(buf, 2);
  mqttPublishStatus();
  mqttPublishVolume();
  mqttPublishPlaylist();
}

void mqttPublishStatus() {
  if(mqttClient.connected()){
    char topic[140], status[255];
    sprintf(topic, "%s%s", MQTT_ROOT_TOPIC, "status");
    sprintf(status, "{\"status\": %d, \"station\": %d, \"name\": \"%s\", \"title\": \"%s\"}", player.mode==PLAYING?1:0, config.store.lastStation, config.station.name, config.station.title);
    mqttClient.publish(topic, 0, true, status);
  }
}

void mqttPublishPlaylist() {
  if(mqttClient.connected()){
    char topic[140], playlist[140];
    sprintf(topic, "%s%s", MQTT_ROOT_TOPIC, "playlist");
    sprintf(playlist, "http://%s%s", WiFi.localIP().toString().c_str(), PLAYLIST_PATH);
    mqttClient.publish(topic, 0, true, playlist);
  }
}

void mqttPublishVolume(){
  if(mqttClient.connected()){
    char topic[140], vol[5];
    sprintf(topic, "%s%s", MQTT_ROOT_TOPIC, "volume");
    sprintf(vol, "%d", config.store.volume);
    mqttClient.publish(topic, 0, true, vol);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (len == 0) return;
  char buf[20];
  strlcpy(buf, payload, len+1);
  if (strcmp(buf, "prev") == 0) {
    player.prev();
    return;
  }
  if (strcmp(buf, "next") == 0) {
    player.next();
    return;
  }
  if (strcmp(buf, "toggle") == 0) {
    player.toggle();
    return;
  }
  if (strcmp(buf, "stop") == 0) {
    player.mode = STOPPED;
    telnet.info();
    return;
  }
  if (strcmp(buf, "start") == 0 || strcmp(buf, "play") == 0) {
    //player.play(config.store.lastStation);
    player.request.station = config.store.lastStation;
    return;
  }
  if (strcmp(buf, "boot") == 0 || strcmp(buf, "reboot") == 0) {
    ESP.restart();
    return;
  }
  if (strcmp(buf, "volm") == 0) {
    player.stepVol(false);
    return;
  }
  if (strcmp(buf, "volp") == 0) {
    player.stepVol(true);
    return;
  }
  int volume;
  if ( sscanf(buf, "vol %d", &volume) == 1) {
    if (volume < 0) volume = 0;
    if (volume > 254) volume = 254;
    player.setVol(volume, false);
    return;
  }
  uint16_t sb;
  if (sscanf(buf, "play %d", &sb) == 1 ) {
    if (sb < 1) sb = 1;
    if (sb >= config.store.countStation) sb = config.store.countStation;
    //player.play(sb);
    player.request.station = sb;
    player.request.doSave = true;
    return;
  }
}

#endif // ifdef MQTT_HOST
