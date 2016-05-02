#include "esp_ssd1306ui.h"


esp_ssd1306ui::esp_ssd1306ui(esp_ssd1306 *display) {
  this->display = display;
}

void esp_ssd1306ui::init() {
  this->display->init();
}

void esp_ssd1306ui::setTargetFPS(byte fps){
  int oldInterval = this->updateInterval;
  this->updateInterval = ((float) 1.0 / (float) fps) * 1000;

  // Calculate new ticksPerFrame
  float changeRatio = oldInterval / this->updateInterval;
  this->ticksPerFrame *= changeRatio;
  this->ticksPerTransition *= changeRatio;
}

// -/------ Automatic controll ------\-

void esp_ssd1306ui::enableAutoTransition(){
  this->autoTransition = true;
}
void esp_ssd1306ui::disableAutoTransition(){
  this->autoTransition = false;
}
void esp_ssd1306ui::setAutoTransitionForwards(){
  this->frameTransitionDirection = 1;
}
void esp_ssd1306ui::setAutoTransitionBackwards(){
  this->frameTransitionDirection = 1;
}
void esp_ssd1306ui::setTimePerFrame(int time){
  this->ticksPerFrame = (int) ( (float) time / (float) updateInterval);
}
void esp_ssd1306ui::setTimePerTransition(int time){
  this->ticksPerTransition = (int) ( (float) time / (float) updateInterval);
}


// -/------ Customize indicator position and style -------\-
void esp_ssd1306ui::setIndicatorPosition(IndicatorPosition pos) {
  this->indicatorPosition = pos;
  this->dirty = true;
}
void esp_ssd1306ui::setIndicatorDirection(IndicatorDirection dir) {
  this->indicatorDirection = dir;
}
void esp_ssd1306ui::setActiveSymbole(const char* symbole) {
  this->activeSymbole = symbole;
  this->dirty = true;
}
void esp_ssd1306ui::setInactiveSymbole(const char* symbole) {
  this->inactiveSymbole = symbole;
  this->dirty = true;
}


// -/----- Frame settings -----\-
void esp_ssd1306ui::setFrameAnimation(AnimationDirection dir) {
  this->frameAnimationDirection = dir;
}
void esp_ssd1306ui::setFrames(FrameCallback* frameFunctions, int frameCount) {
  this->frameCount     = frameCount;
  this->frameFunctions = frameFunctions;
}

// -/----- Overlays ------\-
void esp_ssd1306ui::setOverlays(OverlayCallback* overlayFunctions, int overlayCount){
  this->overlayCount     = overlayCount;
  this->overlayFunctions = overlayFunctions;
}


// -/----- Manuel control -----\-
void esp_ssd1306ui::nextFrame() {
  this->state.frameState = IN_TRANSITION;
  this->state.ticksSinceLastStateSwitch = 0;
  this->frameTransitionDirection = 1;
}
void esp_ssd1306ui::previousFrame() {
  this->state.frameState = IN_TRANSITION;
  this->state.ticksSinceLastStateSwitch = 0;
  this->frameTransitionDirection = -1;
}


// -/----- State information -----\-
esp_ssd1306uiState esp_ssd1306ui::getUiState(){
  return this->state;
}


int esp_ssd1306ui::update(){
  int timeBudget = this->updateInterval - (millis() - this->state.lastUpdate);
  if ( timeBudget <= 0) {
    // Implement frame skipping to ensure time budget is keept
    if (this->autoTransition && this->state.lastUpdate != 0) this->state.ticksSinceLastStateSwitch += ceil(-timeBudget / this->updateInterval);

    this->state.lastUpdate = millis();
    this->tick();
  }
  return timeBudget;
}


void esp_ssd1306ui::tick() {
  this->state.ticksSinceLastStateSwitch++;

  switch (this->state.frameState) {
    case IN_TRANSITION:
        this->dirty = true;
        if (this->state.ticksSinceLastStateSwitch >= this->ticksPerTransition){
          this->state.frameState = FIXED;
          this->state.currentFrame = getNextFrameNumber();
          this->state.ticksSinceLastStateSwitch = 0;
        }
      break;
    case FIXED:
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerFrame){
          if (this->autoTransition){
            this->state.frameState = IN_TRANSITION;
            this->dirty = true;
          }
          this->state.ticksSinceLastStateSwitch = 0;
      }
      break;
  }

  if (this->dirty) {
    this->dirty = false;
    this->display->clear();
    this->drawIndicator();
    this->drawFrame();
    this->drawOverlays();
    this->display->display();
  }
}

void esp_ssd1306ui::drawFrame(){
  switch (this->state.frameState){
     case IN_TRANSITION: {
       float progress = (float) this->state.ticksSinceLastStateSwitch / (float) this->ticksPerTransition;
       int x, y, x1, y1;
       switch(this->frameAnimationDirection){
        case SLIDE_LEFT:
          x = -128 * progress;
          y = 0;
          x1 = x + 128;
          y1 = 0;
          break;
        case SLIDE_RIGHT:
          x = 128 * progress;
          y = 0;
          x1 = x - 128;
          y1 = 0;
          break;
        case SLIDE_UP:
          x = 0;
          y = -64 * progress;
          x1 = 0;
          y1 = y + 64;
          break;
        case SLIDE_DOWN:
          x = 0;
          y = 64 * progress;
          x1 = 0;
          y1 = y - 64;
          break;
       }

       // Invert animation if direction is reversed.
       int dir = frameTransitionDirection >= 0 ? 1 : -1;
       x *= dir; y *= dir; x1 *= dir; y1 *= dir;

       this->dirty |= (this->frameFunctions[this->state.currentFrame])(this->display, &this->state, x, y);
       this->dirty |= (this->frameFunctions[this->getNextFrameNumber()])(this->display, &this->state, x1, y1);
       break;
     }
     case FIXED:
      this->dirty |= (this->frameFunctions[this->state.currentFrame])(this->display, &this->state, 0, 0);
      break;
  }
}

void esp_ssd1306ui::drawIndicator() {
    byte posOfCurrentFrame;

    switch (this->indicatorDirection){
      case LEFT_RIGHT:
        posOfCurrentFrame = this->state.currentFrame;
        break;
      case RIGHT_LEFT:
        posOfCurrentFrame = (this->frameCount - 1) - this->state.currentFrame;
        break;
    }

    for (byte i = 0; i < this->frameCount; i++) {

      const char *image;

      if (posOfCurrentFrame == i) {
         image = this->activeSymbole;
      } else {
         image = this->inactiveSymbole;
      }

      int x,y;
      switch (this->indicatorPosition){
        case TOP:
          y = 0;
          x = 64 - (12 * frameCount / 2) + 12 * i;
          break;
        case BOTTOM:
          y = 56;
          x = 64 - (12 * frameCount / 2) + 12 * i;
          break;
        case RIGHT:
          x = 120;
          y = 32 - (12 * frameCount / 2) + 12 * i;
          break;
        case LEFT:
          x = 0;
          y = 32 - (12 * frameCount / 2) + 12 * i;
          break;
      }

      this->display->drawXbm(x, y, 8, 8, image);
    }
}

void esp_ssd1306ui::drawOverlays() {
 for (int i=0;i<this->overlayCount;i++){
    this->dirty |= (this->overlayFunctions[i])(this->display, &this->state);
 }
}

int esp_ssd1306ui::getNextFrameNumber(){
  int nextFrame = (this->state.currentFrame + this->frameTransitionDirection) % this->frameCount;
  if (nextFrame < 0){
    nextFrame = this->frameCount + nextFrame;
  }
  return nextFrame;
}
