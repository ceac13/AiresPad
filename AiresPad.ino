#define  midichannel 1;                              // MIDI channel from 0 to 15 (+1 in "real world")

struct Hit {
  int value;
  long milliseconds;

  Hit() {
    value = 0;
    milliseconds = 0;
  }

  Hit(int value_, long milliseconds_) {
    value = value;
    milliseconds = milliseconds;
  }
};

char pinAssignments[11] ={'A0','A1','A2','A3','A4','A5','A6','A7','A8','A9', 'A10'};
byte padNote[11] =       { 49 , 53 , 51 , 48 , 43 , 25 , 37 , 38 , 42 , 36  , 4 }; // MIDI notes from 0 to 127 (Mid C = 60)
bool padActive[11] =     {true, true, true, true, true, true, true, true, true, true, true};
bool hihat[11] =         {false, false, false, false, false, false, false, false, false, false, true};
int threshold[11] =      {400, 400, 400, 450, 450, 400, 400, 200, 400, 40, 10}; // Minimum value to get trigger
float gain[11] =      {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}; // multiplier to apply in the analog pin values
int maskTime[11] =      {30, 30, 30, 30, 30, 30, 30, 30, 30, 100, 1}; // Minimum number of cycles to a new trigger. It should to be bigger than the others attributes.
int scanTime =          5; // Time hearing the pad to decide the correct value
float retrigger =       0.6; // New trigger only value is greater than <<retrigger>> * last value
//int maskTime =          30; // Minimum number of cycles to a new trigger. It should to be bigger than the others attributes.
long crossTalk =         1; // Number of milliseconds where cannot have more than one trigger. Highest first
//float gain =            1.0; // multiplier to apply in the analog pin values

int numberOfPads = 11;
int sizeOfCache = 16;
//int padValues[10][16];
Hit padHits[11][16];

int lastTrigger[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Zero when bigger than maskTime
int maxValues[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool shouldTrigger[11] = {false, false, false, false, false, false, false, false, false, false, false};
long startMillis[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool triggered[11] = {false, false, false, false, false, false, false, false, false, false, false};

byte status1;

long lastCycle = 0;
void setup() {
  Serial.begin(57600);   
  
  for (int i = 0; i < numberOfPads; i++) {
    for (int j = 0; j < sizeOfCache; j++) {
      //padValues[i][j] = 0;
      padHits[i][j] = Hit();
    }
  }
}

void loop() {
  //Serial.println(analogRead(0));
  //return ;
  // put your main code here, to run repeatedly:

  // Limpa shouldTrigger
  cleanShouldTriggerArray();

  // Remove o primeiro elemento de todos arrrays de valor e adiciona o valor atual
  updateValuesArray();

  // Adiciona ao shouldTrigger todos os que ultimo valor é maior que o threshold e lastTrigger é 0 (maior que mask time)

  // Atualiza os valores dos em maxValues

  // Para os em shouldTrigger analisa o retrigger e remove os que cairem no filtro
  removeRetriggers();

  // Adiciona os valores dos em shouldTrigger no maxValues
  addMaxValues();
  
  // Incrementa o lastTrigger dos com valores maior que zero no maxValues
  countLastTrigger();

  // Crosstalk....
  //removeCrossTalk(); 

  // Se lastTrigger é > maskTime envia sinal de encerramento do midi e zera lastTrigger e maxValues
  // Se lastTrigger é >= que scanTime e maxValues > 0 envia midi
  triggerMidi();
  
}

void cleanShouldTriggerArray() {
  for (int i = 0; i < numberOfPads; i++) {
    shouldTrigger[i] = false;
  }
}

void updateValuesArray() {
  for (int i = 0; i < numberOfPads; i++) {
    if (padActive[i]) {
      if (hihat[i]) {
        addValueHihat(i);
      } else {
        addValue(i);
      }
    }
  }
}

void putValueInTheEnd(int pin, int value, long milliseconds) {
  for (int i = 1; i < sizeOfCache; i++) {
      padHits[pin][i-1].value = padHits[pin][i].value;
      padHits[pin][i-1].milliseconds = padHits[pin][i].milliseconds;
    }
    padHits[pin][sizeOfCache-1].value = value;
    padHits[pin][sizeOfCache-1].milliseconds = milliseconds;
}

void addValueHihat(int pin) {
  int margin = 10;
  int value = analogRead(pin);
  int velocit = 0;
  //int velocit = value / 8;
  //if (velocit > 127) velocit = 127;
  if (value > 900) velocit = 127;

  int previousValue = padHits[pin][sizeOfCache-1].value;
  int previousVelocit = previousValue / 8;
  if (previousVelocit > 127) previousVelocit = 127;
  
  
  if (fabs(previousVelocit - velocit) > margin) {
    // send midi
    
    putValueInTheEnd(pin, value, millis());
    
    //Serial.println(velocit);
  
    sendMidi(144, padNote[pin], velocit);
  }
}

void addValue(int pin) {
  int value = analogRead(pin);
  value = gain[pin] * value;
  if (value > 0) {
    putValueInTheEnd(pin, value, millis());
  
    analyzeThreshold(pin, value);
    updateMaxValue(pin, value);
  }
}

// Adiciona ao shouldTrigger todos os que ultimo valor é maior que o threshold e lastTrigger é 0 (maior que mask time)
void analyzeThreshold(int pin, int value) {
  if (lastTrigger[pin] == 0 && value >= threshold[pin]) {
    long milliseconds = millis();
    if (!isCrossTalk(milliseconds)) {
      //Serial.println("--------");
      //Serial.println(pin);
      //Serial.println(value);
      //Serial.println(milliseconds);
      shouldTrigger[pin] = true;
      startMillis[pin] = millis();
    }
  }
}

boolean isCrossTalk(long milliseconds) {
  for (int i = 1; i < numberOfPads; i++) {
    if (fabs(milliseconds - startMillis[i]) < crossTalk) {
      return true;
    }
  }
  return false;
}

void updateMaxValue(int pin, int value) {
  if (maxValues[pin] > 0) {
    maxValues[pin] = max(maxValues[pin], value);
  }
}

void removeRetriggers() {
  for (int i = 0; i < numberOfPads; i++) {
    if (shouldTrigger[i]) {
      if (getAvg(i) > retrigger * padHits[i][sizeOfCache - 1].value) {
        shouldTrigger[i] = false;
        startMillis[i] = 0;
      }
    }
  }
}

int getAvg(int pin) {
  int items = 4;
  int initial = sizeOfCache - items - 1;
  int avg = 0;
  
  for (int i = initial; i < sizeOfCache - 1; i++) {
    //avg = avg + padValues[pin][i];
    avg = avg + padHits[pin][i].value;
  }

  return avg/items;
}
/*
void removeCrossTalk() {
  int maxValue = 0;
  int maxx[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  boolean toRemove = false;
  for (int i = 0; i < numberOfPads; i++) {
    if (lastTrigger[i] > (scanTime - crossTalk)) {
      maxx[i] = maxValues[i];
      if (maxValues[i] > maxValue) {
        maxValue = maxValues[i];
      }
    }
    
    if (lastTrigger[i] > scanTime) {
      toRemove = true;
    }
  }

  if (toRemove) {
    for (int i = 0; i < numberOfPads; i++) {
      if (maxx[i] > 0 && maxx[i] < maxValue) {
        lastTrigger[i] = 0;
        maxValues[i] = 0;
        triggered[i] = false;
        shouldTrigger[i] = false;
        startMillis[i] = 0;
      }
    }
  }
}*/

/*
void removeCrossTalk() { // Não está influenciando
  int lastHits[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  int initial = sizeOfCache - crossTalk - 1;
  for (int i = 0; i < numberOfPads; i++) {
    int maxi = 0;
    for (int j = sizeOfCache - 1; j >= initial; j--) {
      if (padValues[i][j] == 0) {
        lastHits[i] = maxi;
        j = -1;
      } else {
        maxi = max(maxi, padValues[i][j]);
      }
    }
  }
  
  int hitPad = -1;
  for (int i = 0; i < numberOfPads; i++) {
    if (lastHits[i] > 0) {
      if (hitPad == -1) {
        hitPad = i;
      } else {
        int valueOld = lastHits[hitPad];
        int valueNew = lastHits[i];
        if (valueNew > valueOld) {
          shouldTrigger[hitPad] = false;
          hitPad = i;
        } else {
          shouldTrigger[i] = false;
        }
      }
    }
  }
}*/

void addMaxValues() {
  for (int i = 0; i < numberOfPads; i++) {
    
    if (shouldTrigger[i]) {
      //maxValues[i] = padValues[i][numberOfPads - 1];
      //maxValues[i] = padHits[i][numberOfPads - 1].value;
      maxValues[i] = padHits[i][sizeOfCache - 1].value;
      
    }
  }
}

void countLastTrigger() {
  for (int i = 0; i < numberOfPads; i++) {
    if (maxValues[i] > 0) {
      lastTrigger[i] = lastTrigger[i] + 1;
    }
  }
}

// Se lastTrigger é > maskTime envia sinal de encerramento do midi e zera lastTrigger e maxValues
// Se lastTrigger é >= que scanTime e maxValues > 0 envia midi
void triggerMidi() {
  for (int i = 0; i < numberOfPads; i++) {
    if (lastTrigger[i] > maskTime[i]) {
      sendMidi(144,padNote[i],0); // Send end midi
      lastTrigger[i] = 0;
      maxValues[i] = 0;
      triggered[i] = false;
      startMillis[i] = 0;
    } 
    else if (lastTrigger[i] >= scanTime && maxValues[i] > 0 && !triggered[i]) {
      triggered[i] = true;
      int velocity = calculateVelocity(maxValues[i], i);
      //Serial.println(maxValues[i]);
      //Serial.println(i);
      //Serial.println("----------------");
      sendMidi(144,padNote[i],velocity);
      //sendMidi(144,padNote[i],0);
    }
  }
}

void sendMidi(byte MESSAGE, byte PITCH, byte VELOCITY) {
  status1 = MESSAGE + midichannel;
  Serial.write(status1);
  Serial.write(PITCH);
  Serial.write(VELOCITY);
}

int calculateVelocity(int value, int pin) {
  // Teste
  int minimo = 70;
  double newValue = value - threshold[pin];
  double dthreshold = threshold[pin];
  double taxa = 127 / (1023 - dthreshold);
  //double taxa = 127 / (700 - dthreshold);
  double temp = taxa * newValue;
  int velocity = int(temp);
  if (velocity == 0) {
    return 1;
  }
  else if (velocity < minimo) {
    return minimo;
  }
  else if (velocity > 127) {
    return 127;
  }
  return velocity;
}

/*
int calculateVelocity(int value, int pin) {
  // Exponencial
  double lambda = 0.007;
  double maxi = exp(1023 * lambda);
  int velocity = exp(value * lambda)/maxi*127;

  return velocity;
}*/
