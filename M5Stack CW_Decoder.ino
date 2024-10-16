///////////////////////////////////////////////////////////////////////
// CW Decoder made by Hjalmar Skovholm Hansen OZ1JHM VER 1.01        //
// Feel free to change, copy or what ever you like but respect       //
// that license is http://www.gnu.org/copyleft/gpl.html              //
// Discuss and give great ideas on                                   //
// https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages //
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// Read more here http://en.wikipedia.org/wiki/Goertzel_algorithm        //
// if you want to know about FFT the http://www.dspguide.com/pdfbook.htm //
///////////////////////////////////////////////////////////////////////////

// M5Stack  
//      rev0.1  2020.03.20  Yado_san
#include <M5Stack.h>
#define MIC 35
#define HIGHT_MAX 240
#define WIDTH_MAX 320
char BtnA_Cnt=0;
char BtnB_Cnt=1;
char BtnC_Cnt=0;
unsigned int sampling_period_us;
unsigned int sampling1_period_us;
unsigned int sampling2_period_us;
long chktime;
int C0 = 0;
int L0 = 0;
int Xc = 30;
int Yc = 40;
int spec_w = 200;
float dmax = 16000.0;
//


int   audioInPin = 35;                // M5Stack ADIN
int   ledPin = 3;                     // M5Stack LED Pin
float magnitude;
float magnitude_[2];
int   magnitudelimit = 100;
int   magnitudelimit_low = 1500;      // M5Stack Please Adjust
int   realstate = LOW;
int   realstatebefore = LOW;
int   filteredstate = LOW;
int   filteredstatebefore = LOW;
///////////////////////////////////////////////////////////
// The sampling frq will be 8928 on a 16 mhz             //
// without any prescaler etc                             //
// because we need the tone in the center of the bins    //
// you can set the tone to 496, 558, 744 or 992          //
// then n the number of samples which give the bandwidth //
// can be (8928 / tone) * 1 or 2 or 3 or 4 etc           //
// init is 8928/558 = 16 *4 = 64 samples                 //
// try to take n = 96 or 128 ;o)                         //
// 48 will give you a bandwidth around 186 hz            //
// 64 will give you a bandwidth around 140 hz            //
// 96 will give you a bandwidth around 94 hz             //
// 128 will give you a bandwidth around 70 hz            //
// BUT remember that high n take a lot of time           //
// so you have to find the compromice - i use 48         //
///////////////////////////////////////////////////////////
float coeff_[6];
float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;
float sampling_freq[2] = {7200.0, 12000.0};                           // Sampling freq. 
float target_freq[6]   = {480.0, 600.0, 720.0, 400.0, 600.0, 800.0};  // Target freq.   
/*
   target_freq[0]=480;         //   480Hz  sampl_freq:  7200Hz  (n=60, bw120Hz)
   target_freq[1]=600;         //   600Hz  sampl_freq:  7200Hz  (n=60, bw120Hz)
   target_freq[2]=720;         //   720Hz  sampl_freq:  7200Hz  (n=60, bw120Hz)
   target_freq[3]=400;         //   400Hz  sampl_freq: 12000Hz  (n=60, bw200Hz)
   target_freq[4]=600;         //   600Hz  sampl_freq: 12000Hz  (n=60, bw200Hz)
   target_freq[5]=800;         //   800Hz  sampl_freq: 12000Hz  (n=60, bw200Hz)
*/
float n=60.0; //// if you change her please change next line also
int testData[60];

//////////////////////////////
// Noise Blanker time which //
// shall be computed so     //
// this is initial          //
//////////////////////////////
int nbtime = 6; /// ms noise blanker
long starttimehigh;
long highduration;
long lasthighduration;
long hightimesavg;
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;
char code[20];
int stop = LOW;
int wpm;
int wpm_before;

////////////////
// init setup //
////////////////
void setup() {
////////////////////////////////////
// The basic goertzel calculation //
////////////////////////////////////
   int k;
   float omega;
   for (int spl = 0; spl < 2; spl++){
      for (int tgt = 0; tgt < 3; tgt++){
         k = (int) (0.5 + ((n * target_freq[spl*3+tgt]) / sampling_freq[spl]));
         omega = (2.0 * PI * k) / n;
         sine = sin(omega);
         cosine = cos(omega);
         coeff_[spl*3+tgt] = 2.0 * cosine;
      }
   }
   coeff = coeff_[1];
   sampling1_period_us = round(1000000 * (1.0 / sampling_freq[0]));
   sampling2_period_us = round(1000000 * (1.0 / sampling_freq[1]));
   sampling_period_us  = sampling1_period_us;

   // M5Stack
   M5.begin(); 
   M5.Lcd.setTextSize(2);
   M5.Lcd.setCursor(30,155);
   M5.Lcd.printf("480Hz");
   M5.Lcd.setCursor(30,175);
   M5.Lcd.printf("600Hz");
   M5.Lcd.setCursor(30,195);
   M5.Lcd.printf("720Hz");
   M5.Lcd.setCursor(30,10);
   M5.Lcd.printf("CW Decoder      OZ1JHM");
   M5.Lcd.drawRect(20,  32, 294, 112,  GREEN);
   M5.Lcd.drawRect(20, 150, 294,  66, YELLOW);
   M5.Lcd.setCursor(40,220);
   M5.Lcd.printf("7.2kHz");
   M5.Lcd.setCursor(135,220);
   M5.Lcd.printf("600Hz");

   Serial.begin(115200);
   pinMode(ledPin, OUTPUT);
}

///////////////
// main loop //
///////////////
void loop() {

   // M5Stack
   chktime = micros();
 //Serial.println(chktime);  // tperiod=  10.1ms ( fsample=7.2kHz, n=60 bw=120Hz ) /  6.7ms ( @fsample=12kHz, n=60 bw=200Hz )
                              //      ex)   8.5ms  /  5.1ms  (without drawChart(); & updateinfolinelcd();)
                              //      20WPM: 60ms/ele  30WPM: 40ms/ele  40WPM: 30ms/ele   (The word PARIS "50elements" is the standard for determing CW code speed)
   M5.update();
   moniButton();

 /////////////////////////////////////
 // The basic where we get the tone //
 /////////////////////////////////////    
   for (char index = 0; index < n; index++){
      unsigned long t = micros();
      testData[index] = analogRead(audioInPin);
      while ((micros() - t) < sampling_period_us) ;
   }
 /////////////////////////////////////////
 // get the magnitude of 3 Target freq. //
 /////////////////////////////////////////
   for (char num =0; num < 3; num++){
      for (char index = 0; index < n; index++){
         float Q0;
         Q0 = coeff_[BtnA_Cnt*3+num] * Q1 - Q2 + (float) testData[index];
         Q2 = Q1;
         Q1 = Q0;
      }
      float magnitudeSquared = (Q1*Q1)+(Q2*Q2)-Q1*Q2*coeff_[BtnA_Cnt*3+num]; // we do only need the realpart //
      magnitude_[num] = sqrt(magnitudeSquared);
      Q2 = 0;
      Q1 = 0;
   }
   /*
   Serial.print("l: ");   //// here you can measure magnitude for setup..
   Serial.print(magnitude_[0]); Serial.println();
   Serial.print("2: ");
   Serial.print(magnitude_[1]); Serial.println(); 
   Serial.print("3: ");
   Serial.print(magnitude_[2]); Serial.println();
   */
   // M5Stack, Draw Spectrums
   drawSpect();
   magnitude = magnitude_[BtnB_Cnt];
 
 ///////////////////////////////////////////////////////////
 // here we will try to set the magnitude limit automatic //
 ///////////////////////////////////////////////////////////
   if (magnitude > magnitudelimit_low){
      magnitudelimit = (magnitudelimit +((magnitude - magnitudelimit)/6)); /// moving average filter
   }
   if (magnitudelimit < magnitudelimit_low)
      magnitudelimit = magnitudelimit_low;
   /*
   Serial.print("L: ");
   Serial.print(magnitudelimit); Serial.println();
   */
 ////////////////////////////////////
 // now we check for the magnitude //
 ////////////////////////////////////
   if(magnitude > magnitudelimit*0.6) // just to have some space up
      realstate = HIGH;
   else
      realstate = LOW;

 /////////////////////////////////////////////////////
 // here we clean up the state with a noise blanker //
 /////////////////////////////////////////////////////
   if (realstate != realstatebefore){
      laststarttime = millis();
   }
   if ((millis()-laststarttime)> nbtime){
      if (realstate != filteredstate){
         filteredstate = realstate;
      }
   }
 ////////////////////////////////////////////////////////////
 // Then we do want to have some durations on high and low //
 ////////////////////////////////////////////////////////////
   if (filteredstate != filteredstatebefore){
      if (filteredstate == HIGH){
         starttimehigh = millis();
         lowduration = (millis() - startttimelow);
      }
      if (filteredstate == LOW){
         startttimelow = millis();
         highduration = (millis() - starttimehigh);
         if (highduration < (2*hightimesavg) || hightimesavg == 0){
            hightimesavg = (highduration+hightimesavg+hightimesavg)/3;
         // now we know avg dit time ( rolling 3 avg)
         }
         if (highduration > (5*hightimesavg) ){
            hightimesavg = highduration+hightimesavg; // if speed decrease fast ..
         }
      }
   }
 ///////////////////////////////////////////////////////////////
 // now we will check which kind of baud we have - dit or dah //
 // and what kind of pause we do have 1 - 3 or 7 pause //
 // we think that hightimeavg = 1 bit //
 ///////////////////////////////////////////////////////////////
   if (filteredstate != filteredstatebefore){
      stop = LOW;
      if (filteredstate == LOW){ //// we did end a HIGH
         if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){    /// 0.6 filter out false dits
            strcat(code,".");
            Serial.print(".");
         }
         if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){
            strcat(code,"-");
            Serial.print("-");
            wpm = (wpm + (1200/((highduration)/3)))/2; //// the most precise we can do 
         }
      }
         if (filteredstate == HIGH){ //// we did end a LOW

         float lacktime = 1;
         if(wpm > 25)lacktime=1.0; /// when high speeds we have to have a little more pause before new letter or new word
         if(wpm > 30)lacktime=1.2;
         if(wpm > 35)lacktime=1.5;

         if (lowduration > (hightimesavg*(2*lacktime)) && lowduration < hightimesavg*(5*lacktime)){ // letter space
            docode();
            code[0] = '\0';
            Serial.print("/");
         }
         if (lowduration >= hightimesavg*(5*lacktime)){ // word space
            docode();
            code[0] = '\0';
            printascii(32);
            Serial.println();
         }
      }
   }

 //////////////////////////////
 // write if no more letters //
 //////////////////////////////
   if ((millis() - startttimelow) > (highduration * 6) && stop == LOW){
      docode();
      code[0] = '\0';
      stop = HIGH;
   }
 /////////////////////////////////////
 // we will turn on and off the LED //
 // and the speaker //
 /////////////////////////////////////
   if(filteredstate == HIGH){
      digitalWrite(ledPin, HIGH);
   // tone(audioOutPin,target_freq);
   }
   else{
      digitalWrite(ledPin, LOW);
   // noTone(audioOutPin);
   }
 //////////////////////////////////
 // the end of main loop clean up//
 /////////////////////////////////
   realstatebefore = realstate;
   lasthighduration = highduration;
   filteredstatebefore = filteredstate;
   if(wpm!=wpm_before){
       if(wpm<100) updateinfolinelcd();
       wpm_before=wpm;
   }
}


////////////////////////////////
// translate cw code to ascii //
////////////////////////////////
void docode(){
   if (strcmp(code,".-") == 0) printascii(65);      // A
   if (strcmp(code,"-...") == 0) printascii(66);    // B 
   if (strcmp(code,"-.-.") == 0) printascii(67);    // C
   if (strcmp(code,"-..") == 0) printascii(68);     // D
   if (strcmp(code,".") == 0) printascii(69);       // E
   if (strcmp(code,"..-.") == 0) printascii(70);    // F
   if (strcmp(code,"--.") == 0) printascii(71);     // G
   if (strcmp(code,"....") == 0) printascii(72);    // H
   if (strcmp(code,"..") == 0) printascii(73);      // I
   if (strcmp(code,".---") == 0) printascii(74);    // J
   if (strcmp(code,"-.-") == 0) printascii(75);     // K
   if (strcmp(code,".-..") == 0) printascii(76);    // L
   if (strcmp(code,"--") == 0) printascii(77);      // M
   if (strcmp(code,"-.") == 0) printascii(78);      // N
   if (strcmp(code,"---") == 0) printascii(79);     // O
   if (strcmp(code,".--.") == 0) printascii(80);    // P
   if (strcmp(code,"--.-") == 0) printascii(81);    // Q
   if (strcmp(code,".-.") == 0) printascii(82);     // R
   if (strcmp(code,"...") == 0) printascii(83);     // S
   if (strcmp(code,"-") == 0) printascii(84);       // T
   if (strcmp(code,"..-") == 0) printascii(85);     // U
   if (strcmp(code,"...-") == 0) printascii(86);    // V
   if (strcmp(code,".--") == 0) printascii(87);     // W
   if (strcmp(code,"-..-") == 0) printascii(88);    // X
   if (strcmp(code,"-.--") == 0) printascii(89);    // Y
   if (strcmp(code,"--..") == 0) printascii(90);    // Z
   if (strcmp(code,".----") == 0) printascii(49);   // 1
   if (strcmp(code,"..---") == 0) printascii(50);   // 2
   if (strcmp(code,"...--") == 0) printascii(51);   // 3
   if (strcmp(code,"....-") == 0) printascii(52);   // 4
   if (strcmp(code,".....") == 0) printascii(53);   // 5
   if (strcmp(code,"-....") == 0) printascii(54);   // 6
   if (strcmp(code,"--...") == 0) printascii(55);   // 7
   if (strcmp(code,"---..") == 0) printascii(56);   // 8
   if (strcmp(code,"----.") == 0) printascii(57);   // 9
   if (strcmp(code,"-----") == 0) printascii(48);   // 0
   if (strcmp(code,"..--..") == 0) printascii(63);  // ?
   if (strcmp(code,".-.-.-") == 0) printascii(46);  // .
   if (strcmp(code,"--..--") == 0) printascii(44);  // ,
   if (strcmp(code,"-.-.--") == 0) printascii(33);  // !
   if (strcmp(code,".--.-.") == 0) printascii(64);  // @
   if (strcmp(code,"---...") == 0) printascii(58);  // :
   if (strcmp(code,"-....-") == 0) printascii(45);  // -
   if (strcmp(code,"-..-.") == 0) printascii(47);   // /
   if (strcmp(code,"-.--.") == 0) printascii(40);   // (
   if (strcmp(code,"-.--.-") == 0) printascii(41);  // )
   if (strcmp(code,".-...") == 0) printascii(95);   // _
   if (strcmp(code,"...-..-") == 0) printascii(36); // $
   if (strcmp(code,"...-.-") == 0) printascii(62);  // >
   if (strcmp(code,".-.-.") == 0) printascii(60);   // <
   if (strcmp(code,"...-.") == 0) printascii(126);  // ~
   if (strcmp(code,"-...-") == 0) printascii(61);   // =
   //////////////////
   // The specials //
   //////////////////
   if (strcmp(code,".-.-") == 0) printascii(3);     // EXT
   if (strcmp(code,"---.") == 0) printascii(4);     // EOT
   if (strcmp(code,".--.-") == 0) printascii(6);    // ACK
}


/////////////////////////////////////
// print the ascii code to the lcd //
/////////////////////////////////////
void printascii(char ascii){
   M5.Lcd.drawChar(ascii,Xc+C0*12, Yc+L0*20);
   C0++;
   if(C0>22){
      C0=0;
      L0++;
      if(L0>4) L0=0;
      M5.Lcd.setCursor(30+C0*10, 40+L0*20);
      M5.Lcd.print("                       ");
   }
}

void updateinfolinelcd(){
/////////////////////////////////////
// here we update the lower line   //
// with the wpm.                   //
/////////////////////////////////////
   M5.Lcd.setCursor(230,220);
   M5.Lcd.printf("WPM:%2d",wpm);
}

void drawSpect(){
/////////////////////////////////////
// here we draw spectrums          //
/////////////////////////////////////
   for (char num = 0; num < 3; num++){
      float d = magnitude_[num];
      if (d > dmax) d = dmax;
      int w = (int)((d / dmax) * spec_w);
      M5.Lcd.fillRect(90,   160+num*20,        w, 6, YELLOW);
      M5.Lcd.fillRect(90+w, 160+num*20, spec_w-w, 6,  BLACK);
   }
}

void moniButton(){
//////////////////////////////
// M5Stack ButtonA,B,C      //
//////////////////////////////
   // Button A
   if (M5.BtnA.wasPressed()) {
      BtnA_Cnt++;
      if(BtnA_Cnt>1) BtnA_Cnt=0;
      if(BtnA_Cnt==0){
         sampling_period_us  = sampling1_period_us;
         M5.Lcd.setCursor(30,155);
         M5.Lcd.printf("480Hz");
         M5.Lcd.setCursor(30,175);
         M5.Lcd.printf("600Hz");
         M5.Lcd.setCursor(30,195);
         M5.Lcd.printf("720Hz");
         drawBtnf();
      }
      if(BtnA_Cnt==1){
         sampling_period_us  = sampling2_period_us;
         M5.Lcd.setCursor(30,155);
         M5.Lcd.printf("400Hz");
         M5.Lcd.setCursor(30,175);
         M5.Lcd.printf("600Hz");
         M5.Lcd.setCursor(30,195);
         M5.Lcd.printf("800Hz");
         drawBtnf();
      }
   }
   // Button B
   if (M5.BtnB.wasPressed()) {
      BtnB_Cnt++;
      if(BtnB_Cnt>2) BtnB_Cnt=0;
      coeff = coeff_[BtnA_Cnt*3+ BtnB_Cnt];
      drawBtnf();
   }
   // Button C
   if (M5.BtnC.wasPressed()) {
      BtnC_Cnt++;
      if(BtnC_Cnt>1) BtnC_Cnt=0;
      M5.Lcd.setCursor( 4,220);
      if(BtnC_Cnt==0)
         M5.Lcd.printf("A");      // ASCII
      else
         M5.Lcd.printf("K");      // Kana(JP)
   }
}

void drawBtnf(){
   M5.Lcd.setCursor(135,220);
   if(BtnA_Cnt==0){
      if(BtnB_Cnt==0) M5.Lcd.printf("480Hz");
      if(BtnB_Cnt==1) M5.Lcd.printf("600Hz");
      if(BtnB_Cnt==2) M5.Lcd.printf("720Hz");
      M5.Lcd.setCursor(40,220);
      M5.Lcd.printf("7.2kHz");
    }
   if(BtnA_Cnt==1){
      if(BtnB_Cnt==0) M5.Lcd.printf("400Hz");
      if(BtnB_Cnt==1) M5.Lcd.printf("600Hz");
      if(BtnB_Cnt==2) M5.Lcd.printf("800Hz");
      M5.Lcd.setCursor(40,220);
      M5.Lcd.printf(" 12kHz");
    }    
}

