enum blinkStates {BANDIT, BANDIT_RESULTS, CONDUIT, CONDUIT_RESULTS, DIAMOND, DIAMOND_RESULTS, RESET_ALL, RESET_RESOLVE};
byte blinkState = BANDIT;

Color teamColors[6] = {RED, ORANGE, YELLOW, GREEN, CYAN, MAGENTA};
byte teamColor = 1;
bool isRevealed = false;
byte currentBid = 1;

byte pointsEarned = 0;
byte diamondFace = 6;
byte diamondSignal;
byte banditFace = 6;
byte banditSignal;

bool showingResults = false;
Timer resultTimer;
#define RESULTS_INTERVAL 1000

byte prizeSignal = 0;
byte winningFace = 6;
//DEBUG VARIABLE, DELETE LATER
byte winningBid = 0;

Timer revealTimer;
#define REVEAL_INTERVAL 2000

#define NO_DIAMOND 6
#define NO_BANDIT 6

enum conduitRevealTypes {NOTHING, WIN_LINE, WIN_PASS, WIN_BANDIT};
byte conduitRevealType = NOTHING;

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  //do loops
  switch (blinkState) {
    case BANDIT:
    case BANDIT_RESULTS:
      banditLoop();
      resetCheck();
      break;
    case CONDUIT:
    case CONDUIT_RESULTS:
      conduitLoop();
      resetCheck();
      break;
    case DIAMOND:
    case DIAMOND_RESULTS:
      diamondLoop();
      resetCheck();
      break;
    case RESET_ALL:
    case RESET_RESOLVE:
      resetLoop();
      break;
  }

  //do display
  switch (blinkState) {
    case BANDIT:
    case BANDIT_RESULTS:
      banditDisplay();
      break;
    case CONDUIT:
    case CONDUIT_RESULTS:
      conduitDisplay();
      break;
    case DIAMOND:
    case DIAMOND_RESULTS:
      diamondDisplay();
      break;
    case RESET_ALL:
    case RESET_RESOLVE:
      resetDisplay();
      break;
  }

  //do communication
  switch (blinkState) {
    case BANDIT:
    case BANDIT_RESULTS:
      setValueSentOnAllFaces((blinkState << 3) + (currentBid));
      break;
    case DIAMOND:
    case DIAMOND_RESULTS:
      setValueSentOnAllFaces((blinkState << 3));
      if (winningFace < 6) {
        setValueSentOnFace((blinkState << 3) + (prizeSignal), winningFace);
      }
      break;
    case RESET_ALL:
    case RESET_RESOLVE:
      setValueSentOnAllFaces(blinkState << 3);
      break;
    case CONDUIT:
    case CONDUIT_RESULTS:
      setValueSentOnAllFaces(blinkState << 3);
      if (diamondFace != NO_DIAMOND && banditFace != NO_BANDIT) {//only send special signals if we've actually got the goods
        setValueSentOnFace(banditSignal, diamondFace);
        setValueSentOnFace(diamondSignal, banditFace);
      }
      break;
  }

  //dump button presses
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonMultiClicked();
  buttonLongPressed();
}

void resetCheck() {
  if (buttonLongPressed()) {
    blinkState = RESET_ALL;
  }

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getBlinkState(getLastValueReceivedOnFace(f)) == RESET_ALL) {
        blinkState = RESET_ALL;
      }
    }
  }
}

void banditLoop() {

  diamondFace = findDiamond();

  //transition to diamond or individually reset
  if (buttonMultiClicked()) {
    blinkState = DIAMOND;
  }

  //reveal/change bid
  if (buttonSingleClicked()) {
    //if I'm currently visible, increment count. Otherwise, just become revealed
    if (isRevealed) {
      currentBid += 1;
      if (currentBid > 3) {
        currentBid = 1;
      }
    }
    revealTimer.set(REVEAL_INTERVAL);
    isRevealed = true;
  }

  if (revealTimer.isExpired()) {
    isRevealed = false;
  }

  //double click cycles through teams
  if (buttonDoubleClicked()) {
    teamColor = (teamColor + 1) % 6;
  }

  //deal with the outcome of my search
  if (diamondFace != NO_DIAMOND) {//I have a diamond neighbor
    byte diamondData = getLastValueReceivedOnFace(diamondFace);
    if (blinkState == BANDIT) {
      //listen for RESULTS and prizes
      if (getBlinkState(diamondData) == DIAMOND_RESULTS) {
        blinkState = BANDIT_RESULTS;

        //did we win?
        if (getPrizeSignal(diamondData) > 0) {
          pointsEarned = getPrizeSignal(diamondData);
          blinkState = CONDUIT_RESULTS;
          beginReveal(pointsEarned);//remember how many points we're earning
          conduitRevealType = WIN_BANDIT;
        } else {
          beginReveal(0);//we did not win, so we don't need to remember anything
        }
      }
    } else {//we're in RESULTS but didn't win. Just wait to see the DIAMOND return
      if (getBlinkState(diamondData) == DIAMOND) {
        blinkState = BANDIT;
      }
    }
  }
}



byte findDiamond() {
  byte diamondFound = NO_DIAMOND;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      if  (getBlinkState(neighborData) == DIAMOND || getBlinkState(neighborData) == DIAMOND_RESULTS) {
        diamondFound = f;
      }
    }
  }
  return diamondFound;
}

bool findBandit(byte face) {

  bool banditFound = false;
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    byte neighborData = getLastValueReceivedOnFace(face);
    if  (getBlinkState(neighborData) == BANDIT || getBlinkState(neighborData) == BANDIT_RESULTS || getBlinkState(neighborData) == CONDUIT || getBlinkState(neighborData) == CONDUIT_RESULTS) {
      banditFound = true;
    }
  }

  return banditFound;
}

Timer resultsTimer;

#define RESULTS_1 2000
#define RESULTS_2 1000
#define RESULTS_3 700
#define RESULTS_4 700

byte resultsMem = 0;

void beginReveal(byte mem) {
  resultsTimer.set(RESULTS_1 + RESULTS_2 + RESULTS_3 + RESULTS_4);
  resultsMem = mem;//used differently by different types of things
}

void conduitLoop() {

  //first step, determine where the diamond is
  diamondFace = findDiamond();

  //now grab the bandit info (if there is any)
  banditFace = NO_BANDIT;
  banditSignal = (blinkState << 3);
  if (findBandit((diamondFace + 3) % 6)) {
    banditFace = (diamondFace + 3) % 6;
    banditSignal = (blinkState << 3) + (getLastValueReceivedOnFace(banditFace) & 7);
  }

  if (blinkState == CONDUIT) {
    //if we have a diamond neighbor, we need to listen for RESULTS
    if (diamondFace != NO_DIAMOND) {//we have a diamond neighbor
      byte diamondData = getLastValueReceivedOnFace(diamondFace);
      if (getBlinkState(diamondData) == DIAMOND_RESULTS) {//ooh, transition time
        blinkState = CONDUIT_RESULTS;
        if (getPrizeSignal(diamondData) > 0) {//sending points
          if (banditFace != NO_BANDIT) {//we've got someone to send points to
            //is it a real bandit or just another conduit?
            if (getBlinkState(getLastValueReceivedOnFace(banditFace)) == BANDIT || getBlinkState(getLastValueReceivedOnFace(banditFace)) == BANDIT_RESULTS) {
              //this is an actual bandit
              prizeSignal = pointsEarned - (pointsEarned / 2);//hand out half rounded up
              beginReveal(prizeSignal);//memory value helps us remember how many points we're giving away
              conduitRevealType = WIN_PASS;
              pointsEarned = pointsEarned / 2;//retain half rounded down
              diamondSignal = (diamondData & 56) + prizeSignal;
            } else {
              //just another conduit
              diamondSignal = (diamondData & 56) + 1;
              beginReveal(0);//memory value of 0 because we're not doing any special animation
              conduitRevealType = WIN_LINE;
            }
          }
        } else {//not sending points
          diamondSignal = (diamondData & 56);
          beginReveal(0);//memory value of 0 because we're not doing any special animation
          conduitRevealType = NOTHING;
        }
      } else {
        diamondSignal = (diamondData & 56);
      }
    }
  } else if (blinkState == CONDUIT_RESULTS) {
    //all we do here is keep blaring the signal
    //and listening to see if we can transition back to CONDUIT
    if (diamondFace != NO_DIAMOND) {//check the diamond
      byte diamondData = getLastValueReceivedOnFace(diamondFace);
      if (getBlinkState(diamondData) == DIAMOND) {
        diamondSignal = diamondData;
        blinkState = CONDUIT;
      }
    } else {//no diamond neighbor, just go back anyway
      blinkState = CONDUIT;
    }
  }

}

void diamondLoop() {
  if (blinkState == DIAMOND) {//determine winner

    byte bidCount[3] = {0, 0, 0};//this tells me how many bids of 1/2/3 we receive
    byte bidLocation[3] = {6, 6, 6};//this tell me where the bid is located, defaulting to 6 because that's nowhere

    //run through the faces and fill out these arrays
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);

        if (getBlinkState(neighborData) == BANDIT || getBlinkState(neighborData) == CONDUIT) {
          byte thisBid = getBid(neighborData);
          bidCount[thisBid - 1] += 1;//increment the count for this type of bid
          bidLocation[thisBid - 1] = f;//set this as the location for that bid. Overwriting is fine because duplicates can't score anyway
        }
      }

      //now determine the winner and where it is located
      winningFace = 6;//default to 6 because that's no one
      //DEBUG VARIABLE
      winningBid = 0;

      if (bidCount[2] == 1) {
        winningFace = bidLocation[2];
        prizeSignal = 3;
        winningBid = 3;
      } else if (bidCount[1] == 1) {
        winningFace = bidLocation[1];
        prizeSignal = 4;
        winningBid = 2;
      } else if (bidCount[0] == 1) {
        winningFace = bidLocation[0];
        prizeSignal = 5;
        winningBid = 1;
      }
    }

    //last thing - go to results state if double clicked
    if (buttonDoubleClicked()) {
      blinkState = DIAMOND_RESULTS;
      beginReveal(winningFace);
    }

  } else {//tell everyone the results
    //here we just listen until all of our neighbors are no longer in BANDIT or CONDUIT
    bool canResolve = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getBlinkState(neighborData) == BANDIT || getBlinkState(neighborData) == CONDUIT) {
          canResolve = false;
        }
      }
    }

    if (canResolve) {//we can go back to just being a DIAMOND
      blinkState = DIAMOND;
    }
  }
}

void resetLoop() {
  if (blinkState == RESET_ALL) {
    //just make sure I don't have any neighbors left in non-RESET states
    blinkState = RESET_RESOLVE;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getBlinkState(neighborData) != RESET_ALL && getBlinkState(neighborData) != RESET_RESOLVE) {
          blinkState = RESET_ALL;
        }
      }
    }

  } else if (blinkState == RESET_RESOLVE) {
    //just make sure there's no one left in RESET_ALL
    blinkState = BANDIT;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getBlinkState(neighborData) == RESET_ALL) {
          blinkState = RESET_RESOLVE;
        }
      }
    }
  }
}

#define SWOOSH_PERIOD 750
#define SWOOSH_HIGHLIGHT 200

#define FADE_DURATION 750

#define DIAMOND_HUE 175
#define DIAMOND_SAT_MAX 100

Timer sparkleTimer;
byte sparkleFace = 0;

void banditDisplay() {

  setColor(OFF);
  switch (currentBid) {
    case 3:
      setColorOnFace(BLUE, 3);
    case 2:
      setColorOnFace(YELLOW, 2);
    case 1:
      setColorOnFace(RED, 1);
      break;
  }

  //  if (resultsTimer.isExpired()) {//normal display
  //    if (revealTimer.getRemaining() < FADE_DURATION) {//default display and fade
  //      //so we start with a default spin
  //      byte sinVal = sin8_C(map(millis() % (SWOOSH_PERIOD * 4), 0, SWOOSH_PERIOD * 4, 0, 255));
  //      byte highlightMax = map(sinVal, 0, 255, 50, SWOOSH_HIGHLIGHT);
  //
  //      byte fadeMax = map(revealTimer.getRemaining(), 0, FADE_DURATION, 0, 255);
  //
  //      FOREACH_FACE(f) {
  //        byte swooshLevel = max(fadeMax, (highlightMax - map((millis() + ((SWOOSH_PERIOD / 6) * f)) % SWOOSH_PERIOD, 0, SWOOSH_PERIOD, 0, highlightMax)));
  //        if (f == diamondFace) {
  //          setColorOnFace(dim(WHITE, swooshLevel), f);
  //        } else {
  //          setColorOnFace(dim(teamColors[teamColor], swooshLevel), f);
  //        }
  //      }
  //    } else {//revealed display
  //      //so we have a default visual
  //      setColor(OFF);
  //      displayPoints(currentBid, 255, false);
  //    }
  //  } else if (resultsTimer.getRemaining() > (RESULTS_2 + RESULTS_3 + RESULTS_4)) {//stage 1
  //    setColor(OFF);
  //    setColorOnFace(dim(teamColors[teamColor], 100), random(5));
  //  } else {//stages 2+3+4
  //    //display bid because you didn't win
  //    setColor(OFF);
  //    displayPoints(currentBid, 255, false);
  //
  //    revealTimer.set(REVEAL_INTERVAL);
  //    isRevealed = true;
  //  }
}



void diamondDisplay() {

  setColor(dim(WHITE, 100));
  if (winningFace < 6) {
    switch (winningBid) {
      case 1:
        setColorOnFace(RED, winningFace);
        break;
      case 2:
        setColorOnFace(YELLOW, winningFace);
        break;
      case 3:
        setColorOnFace(BLUE, winningFace);
        break;
    }
  }

  //  if (resultsTimer.isExpired()) {//normal display
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 255));
  //
  //    if (sparkleTimer.isExpired()) {
  //      sparkleTimer.set(1000);
  //      sparkleFace = random(5);
  //    }
  //
  //    if (sparkleTimer.getRemaining() < 250) {//do a little fade down
  //      byte sat = 100 - map(sparkleTimer.getRemaining(), 0, 250, 0, DIAMOND_SAT_MAX);
  //      setColorOnFace(makeColorHSB(DIAMOND_HUE, sat, 255), sparkleFace);
  //    }
  //  } else if (resultsTimer.getRemaining() > (RESULTS_2 + RESULTS_3 + RESULTS_4)) {//stage 1
  //
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));
  //    setColorOnFace(WHITE, random(5));
  //
  //  } else if (resultsTimer.getRemaining() > (RESULTS_4)) {//stage 2 and 3
  //
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));
  //    setColorOnFace(dim(WHITE, 100), random(5));
  //    if (resultsMem < 6) {
  //      setColorOnFace(WHITE, resultsMem);
  //    }
  //  } else {//stage 4
  //    byte fadeMap = 255 - map(resultsTimer.getRemaining(), 0, RESULTS_4, 0, 155);
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, fadeMap));
  //  }
}

void conduitDisplay() {
  setColor(dim(GREEN, 50));
  if (banditFace != NO_BANDIT && diamondFace != NO_DIAMOND) {//we are sending signals
    byte banditBid = getBid(banditSignal);

    //display bandit bid on diamond face
    switch (banditBid) {
      case 0:
        setColorOnFace(WHITE, diamondFace);
        break;
      case 1:
        setColorOnFace(RED, diamondFace);
        break;
      case 2:
        setColorOnFace(YELLOW, diamondFace);
        break;
      case 3:
        setColorOnFace(BLUE, diamondFace);
        break;
    }

    setColorOnFace(WHITE, banditFace);

  }

  //  if (resultsTimer.isExpired()) {//normal display
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));
  //
  //    if (sparkleTimer.isExpired()) {
  //      sparkleTimer.set(1000);
  //      sparkleFace = random(5);
  //    }
  //
  //    if (sparkleTimer.getRemaining() < 250) {//do a little fade down
  //      byte sat = 100 - map(sparkleTimer.getRemaining(), 0, 250, 0, DIAMOND_SAT_MAX);
  //      setColorOnFace(makeColorHSB(DIAMOND_HUE, sat, 100), sparkleFace);
  //    }
  //
  //    displayPoints(pointsEarned, 255, true);
  //
  //
  //  } else if (resultsTimer.getRemaining() > RESULTS_2 + RESULTS_3 + RESULTS_4) {//stage 1
  //    if (conduitRevealType == WIN_BANDIT) {//pretend to be a bandit
  //      banditDisplay();
  //    } else {//just be a dark conduit
  //      setColor(OFF);
  //      displayPoints(pointsEarned + resultsMem, 100, true);
  //    }
  //  } else if (resultsTimer.getRemaining() > RESULTS_3 + RESULTS_4) {//stage 2
  //    //consistent background
  //    setColor(OFF);
  //    //different foregrounds
  //
  //    switch (conduitRevealType) {
  //      case NOTHING:
  //      case WIN_LINE:
  //        displayPoints(pointsEarned, 100, true);
  //        break;
  //      case WIN_PASS:
  //        //setColorOnFace(dim(WHITE, 100), random(5));
  //        displayPoints(pointsEarned + resultsMem, 100, true);
  //        break;
  //      case WIN_BANDIT:
  //        setColorOnFace(dim(WHITE, 100), random(5));
  //        displayPoints(currentBid, 255, false);
  //        break;
  //    }
  //
  //
  //  } else if (resultsTimer.getRemaining() > RESULTS_4) {//stage 3
  //    //setColor(OFF);
  //
  //    //consistent background
  //    setColor(OFF);
  //
  //    //different foregrounds
  //    byte fadeVal = map(resultsTimer.getRemaining(), RESULTS_4, RESULTS_3 + RESULTS_4, 0, 255);
  //    switch (conduitRevealType) {
  //      case NOTHING:
  //        //do actual blank stuff
  //        setColor(OFF);
  //        displayPoints(pointsEarned, 100, true);
  //        break;
  //      case WIN_LINE:
  //        //sparkle and show points
  //        displayPoints(pointsEarned, 100, true);
  //        break;
  //      case WIN_PASS:
  //        //fade down the points you are losing
  //        displayPoints(pointsEarned + resultsMem, fadeVal, true);
  //        //but maintain the points you're keeping
  //        displayPoints(pointsEarned, 100, true);
  //        break;
  //      case WIN_BANDIT:
  //        //fade down bid, fade up points
  //        setColorOnFace(dim(WHITE, 100), random(5));
  //        displayPoints(currentBid, 255, false);
  //        break;
  //    }
  //
  //
  //  } else {//stage 4
  //    //everyone fades up white background
  //    byte bgFade = 100 - map(resultsTimer.getRemaining(), 0, RESULTS_4, 0, 100);
  //    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, bgFade));
  //    if (conduitRevealType == WIN_BANDIT) {//gain points
  //      byte fadeVal = map(resultsTimer.getRemaining(), 0, RESULTS_4, 0, 255);
  //      displayPoints(currentBid, fadeVal, false);
  //      displayPoints(pointsEarned, 255 - fadeVal, true);
  //    } else {//the rest just need to fade up
  //      displayPoints(pointsEarned, bgFade + 155, true);
  //    }
  //  }
}

void displayPoints(byte points, byte fade, bool oriented) {

  byte orient = diamondFace;
  if (oriented == false) {//it is reverse oriented (outside edge)
    orient += 3;
  }
  switch (points) {
    case 5:
      setColorOnFace(dim(teamColors[teamColor], fade), (orient + 4) % 6);
    case 4:
      setColorOnFace(dim(teamColors[teamColor], fade), (orient + 2) % 6);
    case 3:
      setColorOnFace(dim(teamColors[teamColor], fade), (orient + 5) % 6);
    case 2:
      setColorOnFace(dim(teamColors[teamColor], fade), (orient + 1) % 6);
    case 1:
      setColorOnFace(dim(teamColors[teamColor], fade), orient % 6);
      break;
  }
}

void resetDisplay() {
  banditDisplay();
  setColorOnFace(dim(WHITE, 100), random(5));
  setColorOnFace(dim(WHITE, 100), random(5));
}

byte getBlinkState (byte data) {
  return (data >> 3);
}

byte getBid(byte data) {
  return (data & 7);
}

byte getPrizeSignal(byte data) {
  return (data & 7);
}
