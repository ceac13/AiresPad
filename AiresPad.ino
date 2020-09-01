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

int padS = 100;
int padG = 2.0;

char pinAssignments[11] ={'A0','A1','A2','A3','A4','A5','A6','A7','A8','A9', 'A10'};
byte padNote[11] =       { 4,   49 , 53 , 51 , 48 , 43 ,  1 , 37 , 38 , 18 ,  36  }; // MIDI notes from 0 to 127 (Mid C = 60)
bool padActive[11] =     {true, true, true, true, true, true, true, true, true, true, true};
bool hihat[11] =         {true, false, false, false, false, false, false, false, false, false, false};
int threshold[11] =      {10, padS, padS, padS, padS, padS, padS, padS, padS, padS, 60}; // Minimum value to get trigger
float gain[11] =      {1.0, padG, padG, padG, padG, padG, padG, padG, padG, padG, 2.0}; // multiplier to apply in the analog pin values
int maskTime[11] =      {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 100}; // Minimum number of cycles to a new trigger. It should to be bigger than the others attributes.
int scanTime =          5; // Time hearing the pad to decide the correct value
float retrigger =       0.6; // New trigger only value is greater than <<retrigger>> * last value
//int maskTime =          30; // Minimum number of cycles to a new trigger. It should to be bigger than the others attributes.
long crossTalk =         8; // Number of milliseconds where cannot have more than one trigger. Highest first
double crosstalkRatio = 2.0; // Less than crosstalkRatio * threshold will be removed 
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

int padNeighbours[10][10] = {
  // A0     A1     A2     A3    A4     A5      A6     A7     A8    A9
  { false, false, false, false, false, false, false, false, false, false },  // A0
  { false, true,  true,  false, true,  true,  false, false, false, false },  // A1
  { false, true,  true,  true,  true,  true,  true,  false, false, false },  // A2
  { false, false, true,  true,  false, true,  true,  false, false, false },  // A3
  { false, true,  true,  false, true,  true,  false, true,  true,  false },  // A4
  { false, true,  true,  true,  true,  true,  true,  true,  true,  true  },  // A5
  { false, false, true,  true,  false, true,  true,  false, true,  true  },  // A6
  { false, false, false, false, true,  true,  false, true,  true,  false },  // A7
  { false, false, false, false, true,  true,  true,  true,  true,  true  },  // A8
  { false, false, false, false, false, true,  true,  false, true,  true  }   // A9
};

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
  /*
  Serial.print("pad0;");
  Serial.print(analogRead(0));
  Serial.print(";pad1;");
  Serial.print(analogRead(1));
  Serial.print(";pad2;");
  Serial.print(analogRead(2));
  Serial.print(";pad3;");
  Serial.print(analogRead(3));
  Serial.print(";pad4;");
  Serial.print(analogRead(4));
  Serial.print(";pad5;");
  Serial.print(analogRead(5));
  Serial.print(";pad6;");
  Serial.print(analogRead(6));
  Serial.print(";pad7;");
  Serial.print(analogRead(7));
  Serial.print(";pad8;");
  Serial.print(analogRead(8));
  Serial.println(";");
  
  return ;*/
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
  removeCrosstalk(); 

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
  // max 390 ~ 400
  // min 40
  long maxx = 360;
  long minn = 50;
  int margin = 8;
  
  int value = analogRead(pin);
  int previousValue = padHits[pin][sizeOfCache-1].value;
  
  long velocit = ((value - minn))*127 / ((maxx-minn));
  if (velocit > 118) velocit = 127;
  if (velocit < 0) velocit = 0;
  
  long previousVelocit = (previousValue - minn)*127 / (maxx-minn);
  if (previousVelocit > 127) previousVelocit = 127;
  if (previousVelocit < 0) previousVelocit = 0;

  
  //Serial.println(" ---- " );
  //Serial.println(velocit);
  //Serial.println(previousVelocit);
  
  
  if (fabs(previousVelocit - velocit) > margin) {
    // send midi
    
    putValueInTheEnd(pin, value, millis());
  
    sendMidi(176, padNote[pin], (int) velocit);
  }
  
  // 
}

void addValue(int pin) {
  int value = analogRead(pin);
  /*if (value > 0) {
    Serial.print("Pin: ");
    Serial.print(pin);
    Serial.print(" v: ");
    Serial.println(value);
  }*/
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
    shouldTrigger[pin] = true;
    startMillis[pin] = millis();
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
    avg = avg + padHits[pin][i].value;
  }

  return avg/items;
}

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
      /*Serial.println(i);
      Serial.println(maxValues[i]);
      Serial.println(velocity);
      Serial.println("----------------");*/
      
      sendMidi(144,padNote[i],velocity);
    
    }
  }
}

void removeCrosstalk() {
  // It's specific for 9 pads
  for (int i = 1; i < 10; i++) {
    
    if (maxValues[i] > 0) {
      //Serial.println(startMillis[i]);
      int value = maxValues[i];
      long smillis = startMillis[i];
      double p =  crosstalkRatio * threshold[i];
      
      for (int j = 1; j < 10; j++) {
        
        if (j != i && padNeighbours[i][j] && maxValues[j] > 0) {
          
          int valueJ = maxValues[j];
          long startMillisJ = startMillis[j];
          
          long distance = fabs(smillis - startMillisJ);
          
          if (value < valueJ && p > value && distance <= crossTalk) {
            
            // remove hit
            
            lastTrigger[i] = 0;
            maxValues[i] = 0;
            triggered[i] = false;
            startMillis[i] = 0;
          }
          
        }
      }
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
  int minimo = 80;
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
  int minimo = 80;
  double lambda = 0.007;
  double maxi = exp(1023 * lambda);
  int velocity = exp(value * lambda)/maxi*127;

  if (velocity < minimo) return minimo;
  if (velocity > 127) return 127;
  
  return velocity;
}*/
