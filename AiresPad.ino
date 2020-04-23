#define  midichannel 1;                              // MIDI channel from 0 to 15 (+1 in "real world")

char pinAssignments[9] ={'A0','A1','A2','A3','A4','A5','A6','A7','A8'};
byte padNote[9] =       { 49 , 42 , 51 , 38 , 47 , 45 , 36 , 56, 43 }; // MIDI notes from 0 to 127 (Mid C = 60)
bool padActive[9] =     {true, true, true, true, true, true, true, true, true};
int threshold[9] =      {300, 300, 300, 300, 300, 300, 300, 300, 300}; // Minimum value to get trigger
int scanTime =          5; // Time hearing the pad to decide the correct value
float retrigger =       0.5; // New trigger only value is greater than <<retrigger>> * last value
int maskTime =          40; // Minimum number of cycles to a new trigger. It should to be bigger than the others attributes.
int crossTalk =         1; // Number of cycles where cannot have more than one trigger. Highest first

int numberOfPads = 9;
int sizeOfCache = 32;
int padValues[9][32];

int lastTrigger[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // Zero when bigger than maskTime
int maxValues[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
bool shouldTrigger[9] = {false, false, false, false, false, false, false, false, false};
bool triggered[9] = {false, false, false, false, false, false, false, false, false};

byte status1;
  
void setup() {
  Serial.begin(57600);   
  
  for (int i = 0; i < numberOfPads; i++) {
    for (int j = 0; j < sizeOfCache; j++) {
      padValues[i][j] = 0;
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  // Limpa shouldTrigger
  cleanShouldTriggerArray();

  // Remove o primeiro elemento de todos arrrays de valor e adiciona o valor atual
  updateValuesArray();

  // Adiciona ao shouldTrigger todos os que ultimo valor é maior que o threshold e lastTrigger é 0 (maior que mask time)

  // Atualiza os valores dos em maxValues

  // Para os em shouldTrigger analisa o retrigger e remove os que cairem no filtro
  removeRetriggers();

  // Crosstalk....
  removeCrossTalk(); // Não está fazendo nada por enquanto

  // Adiciona os valores dos em shouldTrigger no maxValues
  addMaxValues();
  
  // Incrementa o lastTrigger dos com valores maior que zero no maxValues
  countLastTrigger();

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
      addValue(i);
    }
  }
}

void addValue(int pin) {
  int value = analogRead(pin);
  for (int i = 1; i < numberOfPads; i++) {
    padValues[pin][i-1] = padValues[pin][i];
  }
  padValues[pin][numberOfPads - 1] = value;

  analyzeThreshold(pin, value);
  updateMaxValue(pin, value);
}

// Adiciona ao shouldTrigger todos os que ultimo valor é maior que o threshold e lastTrigger é 0 (maior que mask time)
void analyzeThreshold(int pin, int value) {
  if (lastTrigger[pin] == 0 && value >= threshold[pin]) {
    shouldTrigger[pin] = true;
  }
}

void updateMaxValue(int pin, int value) {
  if (maxValues[pin] > 0) {
    maxValues[pin] = max(maxValues[pin], value);
  }
}

void removeRetriggers() {
  for (int i = 0; i < numberOfPads; i++) {
    if (shouldTrigger[i]) {
      if (getAvg(i) > retrigger * padValues[i][numberOfPads - 1]) {
        shouldTrigger[i] = false;
      }
    }
  }
}

int getAvg(int pin) {
  int items = 4;
  int initial = sizeOfCache - items - 1;
  int avg = 0;
  
  for (int i = initial; i < sizeOfCache - 1; i++) {
    avg = avg + padValues[pin][i];
  }

  return avg/items;
}

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
}

void addMaxValues() {
  for (int i = 0; i < numberOfPads; i++) {
    if (shouldTrigger[i]) {
      maxValues[i] = padValues[i][numberOfPads - 1];
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
    if (lastTrigger[i] > maskTime) {
      sendMidi(144,padNote[i],0); // Send end midi
      lastTrigger[i] = 0;
      maxValues[i] = 0;
      triggered[i] = false;
    } 
    else if (lastTrigger[i] >= scanTime && maxValues[i] > 0 && !triggered[i]) {
      triggered[i] = true;
      int velocity = calculateVelocity(maxValues[i], i);
      sendMidi(144,padNote[i],velocity);
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
  int minimo = 80;
  double newValue = value - threshold[pin];
  double dthreshold = threshold[pin];
  double taxa = 127 / (1023 - dthreshold);
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
  //return (value/9) + 13;
}

/*
int calculateVelocity(int value, int pin) {
  // Exponencial
  double lambda = 0.007;
  double maxi = exp(1023 * lambda);
  int velocity = exp(value * lambda)/maxi*127;

  return velocity;
}*/
