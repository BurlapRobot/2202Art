#define DEBUG_SPI

#include <FastSPI_LED.h>



#define NUM_LEDS 249
#define LIGHT_BOARD_WIDTH 19
#define LIGHT_BOARD_HEIGHT 13

//#define NUM_LEDS 9
//#define LIGHT_BOARD_WIDTH 3
//#define LIGHT_BOARD_HEIGHT 3


float f, p, q, t, vs, vsf, hsv_i;
void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
  int hsv_i;

  if( s == 0 ) {
    // achromatic (grey)
    *r = *g = *b = v;
    return;
  }

  h /= 60;			// sector 0 to 5
  hsv_i = floor( h );
  f = h - hsv_i;			// factorial part of h
  //p = v * ( 1 - s );
  //q = v * ( 1 - s * f );
  //t = v * ( 1 - s * ( 1 - f ) );

  vs = v * s;
  vsf = vs * f;

  p = v - vs;
  q = v - vsf;
  t = v - vs + vsf;

  switch( hsv_i ) {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  default:		// case 5:
    *r = v;
    *g = p;
    *b = q;
    break;
  }

}
struct CHSV { float h; float s; float v; 
  CHSV(float h, float s, float v) : h(h), s(s), v(v) {}
};

// Sometimes chipsets wire in a backwards sort of way
struct CRGB { unsigned char r; unsigned char g; unsigned char b; 
  CRGB(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
  CRGB(CHSV hsv) : r(0), g(0), b(0)
  {
    float rf, gf, bf;
    HSVtoRGB(&rf, &gf, &bf, hsv.h, hsv.s, hsv.v);
    r = round(rf * 255);
    g = round(gf * 255);
    b = round(bf * 255);
  }
};
// struct CRGB { unsigned char r; unsigned char g; unsigned char b; };
struct CRGB *leds;


//#define PIN 4


class Painter {
  public:
    Painter(int width, int height)
    {
      m_width = width;
      m_height = height;
    }
    virtual void setPixelColor(int x, int y, CRGB color) =0;
    virtual void setIndexColor(int i, CRGB color) =0;
    void draw()
    {
      FastSPI_LED.show();
    }
    int getWidth() { return m_width; }
    int getHeight() { return m_height; }
    int getIndexCount() { return m_width * m_height +2; }
  protected:
    int m_width;
    int m_height;
    int getIndex(int x, int y)
    {
      // The strand starts at the bottom right and snakes up and to the 
      // left, like so (assuming width of 4):
      //
      //  ^  11---10---09---08
      //  |  04---05---06---07
      //  y  03---02---01---00
      //   x --->
      // 
      // So, y*width will tell us the minimum index it could be.  Since
      // the rows alternate ordering, if the row is even, the additional
      // offset is width-x-1.
      int index = y * m_width + 1;
      if (y%2==1) {
          index += x;
      }else{
          index += (m_width-x-1);
      }
      return index;
    }
    int getY(int index)
    {
      index++;
      return floor((index)/ m_width);
    }
    int getX(int index)
    {
      index++;
      if (getY(index) % 2 == 0)
      {
        return index % m_width;
      }
      else
      {
        return m_width - (index % m_width) - 1;
      }
    }
};

class NormalPainter : public Painter
{
  public:
    NormalPainter(int width, int height) : Painter(width, height) {}
    void setPixelColor(int x, int y, CRGB color)
    {
      int index = getIndex(x, y);
      setIndexColor(index, color);
    } 
    void setIndexColor(int i, CRGB color)
    {
      leds[i].r = color.r;
      leds[i].g = color.g;
      leds[i].b = color.b;
    }
};

class Transition : public Painter
{
  protected:
    float m_percentComplete;
  public:
    Transition(int width, int height) : Painter(width, height)
    {
      m_percentComplete = 0.0;
    }
    void setPercentComplete(float percentComplete)
    {
      m_percentComplete = percentComplete;
    }
};

class WipeTransition : public Transition
{
  private:
    int m_mode;
  public:
    // Mode: 0= left to right, 1 = right to left, 2 = top to bottom, 3 = bottom to top
    WipeTransition(int width, int height, int mode)
    : Transition(width, height)
    {
      m_mode = mode;
    }
    void setPixelColor(int x, int y, CRGB color)
    {
      switch(m_mode)
      {
        case 0:
        default:
          int threshold = ceil(m_percentComplete * m_width);
          if(x < threshold)
          {
            int i = getIndex(x,y);
            leds[i].r = color.r;
            leds[i].g = color.g;
            leds[i].b = color.b;
          }
      }
    }
    void setIndexColor(int i, CRGB color)
    {
      setPixelColor(getX(i), getY(i), color);
    }
};

class FadeTransition : public Transition
{
  public:
    FadeTransition(int width, int height) : Transition(width, height) {};
    
    void setPixelColor(int x, int y, CRGB color)
    {
      int i = getIndex(x, y);
      setIndexColor(i, color);
    }
    void setIndexColor(int i, CRGB color)
    {
      leds[i].r = m_percentComplete * color.r + (1.0 - m_percentComplete) * leds[i].r;
      leds[i].g = m_percentComplete * color.g + (1.0 - m_percentComplete) * leds[i].g;
      leds[i].b = m_percentComplete * color.b + (1.0 - m_percentComplete) * leds[i].b;
    }
};

class Program {
  public:
    virtual void draw(Painter*) =0;
};

class ProgramManager
{
  private:
    int m_width;
    int m_height;
    int m_currentProgramIndex;
    int m_programCount;
    Program** m_programs;
    int m_transitionCount;
    Transition** m_transitions;
    NormalPainter m_normalPainter;
    bool m_transitioning;
    unsigned long m_transitionStart;
    int m_transitionDuration;
    int m_currentTransitionIndex;
  public:
    ProgramManager(int width, int height, Program* programs[], int programCount, Transition* transitions[], int transitionCount)
    : m_normalPainter(width, height)
    {
        m_width = width;
        m_height = height;
	m_currentProgramIndex = 0;
	m_programCount = programCount;
        m_programs = programs;
        m_transitionCount = transitionCount;
        m_transitions = transitions;
        m_transitioning = false;
        m_transitionStart = 0;
        m_transitionDuration = 0;
        m_currentTransitionIndex = 0;
    }
    
    void transitionImmediately(void)
    {
      m_currentProgramIndex = getNextProgramIndex();
    }
    
    void transitionOverTime(int transitionDuration)
    {
      if (m_transitioning)
      {
        Serial.println("Ignoring begin transition");
      }
      else
      {
        Serial.println("Begin Transition");
        m_transitioning = true;
        m_transitionStart = millis();
        m_transitionDuration = transitionDuration;
      }
    }
      
    void draw(void)
    {      
      if(m_programCount > 0)
      {
        m_programs[m_currentProgramIndex]->draw(&m_normalPainter);
        
        if(m_transitioning && m_transitionCount > 0)
        {          
          float percentComplete = float(millis() - m_transitionStart) / m_transitionDuration;

          // Move on to the next program after the transition 
          if (percentComplete >= 1.0)
          {
            m_currentProgramIndex = getNextProgramIndex();
            m_transitioning = false;
            Serial.println("End Transition");
          }
          else
          {          
            Transition* currentTransition = m_transitions[m_currentTransitionIndex];
            currentTransition->setPercentComplete(percentComplete);
            
            int nextProgramIndex = getNextProgramIndex();
            m_programs[nextProgramIndex]->draw(currentTransition);
            
            currentTransition->draw();
          }
        }
        else
        {
          m_normalPainter.draw();
        }
      }
    }
  private:
    int getNextProgramIndex(void)
    {
      return (m_currentProgramIndex + 1) % m_programCount;
    }
    int getNextTransitionIndex(void)
    {
      return (m_currentTransitionIndex + 1) % m_transitionCount;
    }
};

class SinWave : public Program
{
  private:
    CRGB m_fg, m_bg;
    int m_durationMs;
    float m_period;
    float m_amplitude;
    unsigned long m_timeStart;
    
  public:
  SinWave(CRGB fg, CRGB bg, int durationMs) : m_fg(fg), m_bg(bg), m_durationMs(durationMs)
  {
    m_period = 2.0 * 3.1415;
    m_amplitude = 0.8;
    m_timeStart = millis();
  }
  void draw(Painter* painter)
  {
    if(millis() > m_timeStart + m_durationMs) { m_timeStart += m_durationMs; }
    float time_percent = float(millis() - m_timeStart) / m_durationMs;
    float offset = m_period * time_percent;
    
    // BG
    for (int i=0; i<painter->getIndexCount(); ++i)
    {
       painter->setIndexColor(i, m_bg); 
    }
    // FG
    for (int x=0; x<painter->getWidth(); x++)
    {
      float x_percent = 1.0 * x / painter->getWidth();
      int y = int((sin(m_period*x_percent+offset)*m_amplitude/2.0+0.5)*painter->getHeight());
      painter->setPixelColor( x, y, m_fg );
    }
  } 
};

class SolidColor : public Program
{
  private:
    CRGB m_color;
  public:
    SolidColor(CRGB color) : m_color(color)
    {
    }
    void draw(Painter* painter)
    {
      for (int i=0; i<painter->getIndexCount(); ++i)
      {
         painter->setIndexColor(i, m_color); 
      }
    }
};

class HSVTest : public Program
{
  private:
    int m_durationMs;
    unsigned long m_timeStart;
  
  public:
  HSVTest(int durationMs) : m_durationMs(durationMs)
  {
    m_timeStart = millis();
  }
  
  
  void draw(Painter* painter)
  {
    if(millis() > m_timeStart + m_durationMs) { m_timeStart += m_durationMs; }
    float time_percent = float(millis() - m_timeStart) / m_durationMs;
    
    
    for (int i=0; i<painter->getIndexCount(); ++i)
    {
       float indexPercent = 1.0 * i / painter->getIndexCount();
       float h = ((indexPercent + time_percent) * 360.0);
       if(h > 360) { h-= 360; }
       CRGB color = CRGB(CHSV(h, 1.0, 1.0));
       painter->setIndexColor(i, color); 
    }
    
    
  }
};

class RainbowSpiral : public Program
{
  private:
    int m_durationMs;
    unsigned long m_timeStart;
  
  public:
  RainbowSpiral(int durationMs) : m_durationMs(durationMs)
  {
    m_timeStart = millis();
  }
  
  
  void draw(Painter* painter)
  {
    if(millis() > m_timeStart + m_durationMs) { m_timeStart += m_durationMs; }
    float time_percent = float(millis() - m_timeStart) / m_durationMs;
    
    int x, y, x_max_index, y_max_index, x_min_index, y_min_index, x_dir, y_dir;
    
    //index 0 is x, index 1 is y
    int axis_dir[2] = {1, 0};
    int min_index[2] = {0, 1};
    int max_index[2] = {LIGHT_BOARD_WIDTH-1,LIGHT_BOARD_HEIGHT-1};
    int other_axis;
    
    x = y = x_min_index = 0;
    y_min_index = 1;
    x_max_index = LIGHT_BOARD_WIDTH - 1;
    y_max_index = LIGHT_BOARD_HEIGHT - 1;
    x_dir = 1;
    y_dir = 0;
        
    for (int i=0; i<painter->getIndexCount(); ++i)
    {
      
       float indexPercent = 1.0 * i / painter->getIndexCount();
       float h = ((indexPercent + time_percent) * 360.0);
       if(h > 360) { h-= 360; }
       CRGB color = CRGB(CHSV(h, 1.0, 1.0));
       
       painter->setPixelColor(x, y, color);
/*
       for (int axis=0;axis <= 1) {
          other_axis = !axis;
          
       }
*/
        
       if (x_dir > 0) {
         if (x + x_dir > x_max_index) {
            x_dir = 0;
            y_dir = 1;
            x_max_index -= 1;
         }
       }
       if (x_dir < 0) {
         if (x + x_dir < x_min_index) {
            x_dir = 0;
            y_dir = -1;
            x_min_index += 1;
         }
       }         
       if (y_dir > 0) {
         if (y + y_dir > y_max_index) {
            y_dir = 0;
            x_dir = -1;
            y_max_index -= 1;
         }
       }
       if (y_dir < 0) {
        if (y + y_dir < y_min_index) {
            y_dir = 0;
            x_dir = 1;
            y_min_index += 1;
        }
       }
       x += x_dir;
       y += y_dir;
       
    }       
  }
};



class Elena : public Program
{
  private:
    int m_pixelCount;
    uint8_t m_pixelUpdateOrder[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
    uint8_t m_pixelUpdateOrderIndex;
    
    int m_updateIntervalMs;
    unsigned long m_timeStart;
    unsigned long m_fullRefreshCount;
    
    CHSV m_baseColor;
    float m_baseAdjustmentH;
    float m_baseAdjustmentS;

    // These are 0-1, but represented as an integer out of 65535
//    uint8_t m_pixelAmplitudeH[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
//    uint8_t m_pixelAmplitudeS[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
//    uint8_t m_pixelAmplitudeV[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
//    uint8_t m_pixelPeriodH[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
//    uint8_t m_pixelPeriodS[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];
//    uint8_t m_pixelPeriodV[LIGHT_BOARD_WIDTH*LIGHT_BOARD_HEIGHT];

    float m_maxPixelAmplitudeH;
    float m_maxPixelAmplitudeS;
    float m_maxPixelAmplitudeV;
    uint8_t m_maxPixelPeriodH;
    uint8_t m_maxPixelPeriodS;
    uint8_t m_maxPixelPeriodV;

  public:
    Elena(int pixelCount, int updateIntervalMs) : 
      m_pixelCount(pixelCount),
      m_pixelUpdateOrderIndex(0),
      m_updateIntervalMs(updateIntervalMs),
      m_fullRefreshCount(0),
      m_baseColor(random(360.0), 0.8, 0.6),
      m_baseAdjustmentH(0.05),
      m_baseAdjustmentS(0.01),
      m_maxPixelAmplitudeH(10.0),
      m_maxPixelAmplitudeS(0.2),
      m_maxPixelAmplitudeV(0.4),
      m_maxPixelPeriodH(50),
      m_maxPixelPeriodS(100),
      m_maxPixelPeriodV(100)
    {
      m_timeStart = millis();
      
      // Shuffle pixel orders using Knuth shuffle
      for(int i=0; i<m_pixelCount; ++i)
      {
        m_pixelUpdateOrder[i] = i; 
      }
      // i is the number of items remaining to be shuffled.
      for (int i = m_pixelCount; i > 1; --i) {
        // Pick a random element to swap with the i-th element.
        int j = random(m_pixelCount);
        // Swap array elements.
        uint8_t tmp = m_pixelUpdateOrder[j];
        m_pixelUpdateOrder[j] = m_pixelUpdateOrder[i-1];
        m_pixelUpdateOrder[i-1] = tmp;
      }
      
      // Assign random amplitudes and periods for each pixel
      for(int i=0; i<m_pixelCount; ++i)
      {
//        // These are 0-1, but represented as an integer out of 65535
//        m_pixelAmplitudeH[i] = random(65535);
//        m_pixelAmplitudeS[i] = random(65535);
//        m_pixelAmplitudeV[i] = random(65535);
//        m_pixelPeriodH[i] = random(65535);
//        m_pixelPeriodS[i] = random(65535);
//        m_pixelPeriodV[i] = random(65535);
        
      }
    }

    void incrementBaseColor()
    {
      // Rotate the hue
      m_baseColor.h += m_baseAdjustmentH;
      
      // Randomly move saturation, but not as big as the pixel max
      m_baseColor.s += 0.0001 * random(-10000, 10000) * m_baseAdjustmentH;
      
      // Correct for out of range
      if (m_baseColor.h > 360.0) 
      {
        m_baseColor.h -= 360.0;
      }
      m_baseColor.s = constrain(m_baseColor.s, 0.0, 1.0);
    
      // The base value doesn't change
    }
  
    void draw(Painter* painter)
    {
      
      if(millis() > m_timeStart + m_updateIntervalMs)
      {
        // We've crossed the threshold, so move the last bar forward
        m_timeStart += m_updateIntervalMs;
        
        // Get the current pixel index
        int currentPixelIndex = m_pixelUpdateOrder[m_pixelUpdateOrderIndex];
        
        // Start by updating the base color
        incrementBaseColor();
        CHSV color = CHSV(m_baseColor);
        
//        // Lookup the pixel's personality, and convert to -1.0 - 1.0
//        float h_amp_f = 1.0 - 2.0 * m_pixelAmplitudeH[currentPixelIndex] / 65535.0;
//        float s_amp_f = 1.0 - 2.0 * m_pixelAmplitudeS[currentPixelIndex] / 65535.0;
//        float v_amp_f = 1.0 - 2.0 * m_pixelAmplitudeV[currentPixelIndex] / 65535.0;
//        float h_per_f = 1.0 - 2.0 * m_pixelPeriodH[currentPixelIndex] / 65535.0;
//        float s_per_f = 1.0 - 2.0 * m_pixelPeriodS[currentPixelIndex] / 65535.0;
//        float v_per_f = 1.0 - 2.0 * m_pixelPeriodV[currentPixelIndex] / 65535.0;
//        
//        // Calculate the offset from the base color
//        color.h += m_maxPixelAmplitudeH * sin(2.0 * PI * m_fullRefreshCount / m_maxPixelPeriodH * h_per_f );
//        color.s += m_baseColor.s + m_maxPixelAmplitudeS * sin(2.0 * PI * m_fullRefreshCount / m_maxPixelPeriodS * s_per_f );
//        color.v += m_baseColor.v + m_maxPixelAmplitudeV * sin(2.0 * PI * m_fullRefreshCount / m_maxPixelPeriodV * v_per_f );
        
        // Correct for out of range
        if (color.h > 360.0) { 
          color.h -= 360.0; 
        }
        if (color.h < 0.0) { 
          color.h += 360.0; 
        }
        color.s = constrain(color.s, 0.0, 0.5);
        color.v = constrain(color.v, 0.0, 1.0);
        
        // Set the pixel color
        painter->setIndexColor(currentPixelIndex, color); 
        
        // Increment to the next pixel
        m_pixelUpdateOrderIndex++;
       
        // Increment to the next full refresh
        if (m_pixelUpdateOrderIndex > m_pixelCount)
        {
          m_pixelUpdateOrderIndex = 0;
          m_fullRefreshCount++;
        }
        
        Serial.print(m_pixelUpdateOrderIndex);
        Serial.print(" to ");
        Serial.print(currentPixelIndex);
        Serial.print(" with hsv color ");
        Serial.print(color.h);
        Serial.print(",");
        Serial.print(color.s);
        Serial.print(",");
        Serial.print(color.v);
        Serial.println();
        
      }
            
    }
};

//void showOneAtATime()
//{
//  // one at a time
//  for(int j = 0; j < 3; j++) { 
//    for(int i = 0 ; i < NUM_LEDS; i++ ) {
//      memset(leds, 0, NUM_LEDS * 3);
//      switch(j) { 
//        case 0: leds[i].r = 255; break;
//        case 1: leds[i].g = 255; break;
//        case 2: leds[i].b = 255; break;
//      }
//      FastSPI_LED.show();
//      delay(1);
//    }
//  }
//}

//void showGrowingBars()
//{
//  // growing/receeding bars
//  for(int j = 0; j < 3; j++) { 
//    memset(leds, 0, NUM_LEDS * 3);
//    for(int i = 0 ; i < NUM_LEDS; i++ ) {
//      switch(j) { 
//        case 0: leds[i].r = 255; break;
//        case 1: leds[i].g = 255; break;
//        case 2: leds[i].b = 255; break;
//      }
//      FastSPI_LED.show();
//      delay(10);
//    }
//    for(int i = NUM_LEDS-1 ; i >= 0; i-- ) {
//      switch(j) { 
//        case 0: leds[i].r = 0; break;
//        case 1: leds[i].g = 0; break;
//        case 2: leds[i].b = 0; break;
//      }
//      FastSPI_LED.show();
//      delay(1);
//    }
//  }
//}

//void showFadeInFadeOut()
//{
//  // Fade in/fade out
//  for(int j = 0; j < 3; j++ ) { 
//    memset(leds, 0, NUM_LEDS * 3);
//    for(int k = 0; k < 256; k++) { 
//      for(int i = 0; i < NUM_LEDS; i++ ) {
//        switch(j) { 
//          case 0: leds[i].r = k; break;
//          case 1: leds[i].g = k; break;
//          case 2: leds[i].b = k; break;
//        }
//      }
//      FastSPI_LED.show();
//      delay(3);
//    }
//    for(int k = 255; k >= 0; k--) { 
//      for(int i = 0; i < NUM_LEDS; i++ ) {
//        switch(j) { 
//          case 0: leds[i].r = k; break;
//          case 1: leds[i].g = k; break;
//          case 2: leds[i].b = k; break;
//        }
//      }
//      FastSPI_LED.show();
//      delay(3);
//    }
//  }
//}

  Program* programs[] = {
    new RainbowSpiral(10000),
    new Elena(LIGHT_BOARD_HEIGHT*LIGHT_BOARD_WIDTH, 1),
    new HSVTest(10000),
    new Elena(LIGHT_BOARD_HEIGHT*LIGHT_BOARD_WIDTH, 1)
    //new SinWave( CRGB(255, 64, 255), CRGB(8, 32, 16), 2000 ),
    //new SinWave( CRGB(255, 255, 32), CRGB(12, 0, 4), 1000 ),
    //new SolidColor(CRGB(255, 0, 255)),
    //new SolidColor(CRGB(0, 255, 255))
  };
  
  Transition* transitions[] = {
    new FadeTransition(LIGHT_BOARD_WIDTH, LIGHT_BOARD_HEIGHT)
//    new WipeTransition(LIGHT_BOARD_WIDTH, LIGHT_BOARD_HEIGHT, 0)
  };
  
  ProgramManager pm = ProgramManager(LIGHT_BOARD_WIDTH, LIGHT_BOARD_HEIGHT, programs, 4, transitions, 1);

void setup()
{
  Serial.begin(115200);
  Serial.println("Hello");
  
  FastSPI_LED.setLeds(NUM_LEDS);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_TM1809);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_LPD6803);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_HL1606);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_595);
  FastSPI_LED.setChipset(CFastSPI_LED::SPI_WS2801);

  //FastSPI_LED.setPin(PIN); // Irrelevant for ws2801
  
  FastSPI_LED.setDataRate(3);
  
  FastSPI_LED.init();
  FastSPI_LED.start();

  leds = (struct CRGB*)FastSPI_LED.getRGBData();
  

}


unsigned long loopStart = 0;
void loop()
{ 
  if (millis() > loopStart + 10000)
  {
    loopStart = millis();
    pm.transitionOverTime(5000); 
  }
  
  pm.draw();
//  showOneAtATime();
//  showSinWave();
//  showGrowingBars();
//  showFadeInFadeOut();
}
