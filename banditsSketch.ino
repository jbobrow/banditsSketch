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

byte orientationFace = 6;
byte prizeSignal = 0;
byte winningFace = 6;

Timer revealTimer;
#define REVEAL_INTERVAL 2000

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  //listen for reset signals
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

  //do loops
  switch (blinkState) {
    case BANDIT:
    case BANDIT_RESULTS:
      banditLoop();
      break;
    case CONDUIT:
    case CONDUIT_RESULTS:
      conduitLoop();
      break;
    case DIAMOND:
    case DIAMOND_RESULTS:
      diamondLoop();
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
      if (diamondFace < 6) {
        setValueSentOnFace(banditSignal, diamondFace);
      }
      if (banditFace < 6) {
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

void banditLoop() {

  orientationFace = 6;

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

  //now let's handle the win/lose signal stuff
  //so we need to find the treasure face that we are touching and listen for it to tell us stuff

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      if  (getBlinkState(neighborData) == DIAMOND || getBlinkState(neighborData) == DIAMOND_RESULTS) {
        orientationFace = f;
      }
    }
  }

  //deal with the outcome of my search
  if (orientationFace < 6) {//I have a diamond neighbor
    byte diamondData = getLastValueReceivedOnFace(orientationFace);
    if (blinkState == BANDIT) {
      //listen for RESULTS and prizes
      if (getBlinkState(diamondData) == DIAMOND_RESULTS) {
        blinkState = BANDIT_RESULTS;

        //did we win?
        if (getPrizeSignal(diamondData) > 0) {
          pointsEarned = getPrizeSignal(diamondData);
          blinkState = CONDUIT_RESULTS;
          beginReveal(pointsEarned);//remember how many points we're earning
        } else {
          beginReveal((orientationFace + 3) % 6);//the orientation face
        }
      }
    } else {//we're in RESULTS but didn't win. Just wait to see the DIAMOND return
      if (getBlinkState(diamondData) == DIAMOND) {
        blinkState = BANDIT;
      }
    }
  }
}

Timer resultsTimer;
#define RESULTS_1 1500
#define RESULTS_2 1000
#define RESULTS_3 500
#define RESULTS_4 250

byte resultsMem = 0;

void beginReveal(byte mem) {
  resultsTimer.set(RESULTS_1 + RESULTS_2 + RESULTS_3 + RESULTS_4);
  resultsMem = mem;//used differently by different types of things
}

void conduitLoop() {

  //first step, determine where the diamond is
  diamondFace = 6;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getBlinkState(neighborData) == DIAMOND || getBlinkState(neighborData) == DIAMOND_RESULTS) {//hey, a diamond!
        diamondFace = f;
        orientationFace = f;
        //diamondSignal = neighborData;
      }
    }
  }

  //now grab the bandit info (if there is any)
  banditFace = 6;//default no face
  banditSignal = (blinkState << 3);//default signal for no bandit

  if (diamondFace != 6) {
    banditFace = (diamondFace + 3) % 6;

    if (!isValueReceivedOnFaceExpired(banditFace)) {//oh, I've got a neighbor there
      byte neighborData = getLastValueReceivedOnFace(banditFace);

      if (getBlinkState(neighborData) == BANDIT || getBlinkState(neighborData) == BANDIT_RESULTS || getBlinkState(neighborData) == CONDUIT || getBlinkState(neighborData) == CONDUIT_RESULTS) {
        //the other neighbor is in fact a bandit (or a conduit)
        banditSignal = (blinkState << 3) + (neighborData & 7);
      }
    }
  }

  if (blinkState == CONDUIT) {
    //so we just need to look for DIAMOND_RESULTS
    byte diamondData = getLastValueReceivedOnFace(diamondFace);
    if (getBlinkState(diamondData) == DIAMOND_RESULTS) {//transition
      //this is where we do the big reveal thingy
      blinkState = CONDUIT_RESULTS;
      if (getPrizeSignal(diamondData) > 0) {
        //so this is where we send points

        if (getBlinkState(getLastValueReceivedOnFace(banditFace)) == BANDIT || getBlinkState(getLastValueReceivedOnFace(banditFace)) == BANDIT_RESULTS) {
          //this is a real bandit, so we should pass points
          prizeSignal = pointsEarned - (pointsEarned / 2);//hand out half rounded up
          beginReveal(prizeSignal);//memory value helps us remember how many points we're giving away

          pointsEarned = pointsEarned / 2;//retain half rounded down
          diamondSignal = (diamondData & 56) + prizeSignal;
        } else {
          //just a conduit, throw them a 1
          diamondSignal = (diamondData & 56) + 1;
          beginReveal(0);//memory value of 0 because we're not doing any special animation


        }
      }
    } else {//just conduit the data
      diamondSignal = diamondData;
    }
  } else if (blinkState == CONDUIT_RESULTS) {
    //all we do here is keep blaring the signal
    //then we listen to transition back
    byte diamondData = getLastValueReceivedOnFace(diamondFace);
    if (getBlinkState(diamondData) == DIAMOND) {
      diamondSignal = diamondData;
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
      if (bidCount[2] == 1) {
        winningFace = bidLocation[2];
        prizeSignal = 3;
      } else if (bidCount[1] == 1) {
        winningFace = bidLocation[1];
        prizeSignal = 4;
      } else if (bidCount[0] == 1) {
        winningFace = bidLocation[0];
        prizeSignal = 5;
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

void banditDisplay() {

  if (resultsTimer.isExpired()) {//normal display
    if (revealTimer.getRemaining() < FADE_DURATION) {//default display and fade
      //so we start with a default spin
      byte sinVal = sin8_C(map(millis() % (SWOOSH_PERIOD * 4), 0, SWOOSH_PERIOD * 4, 0, 255));
      byte highlightMax = map(sinVal, 0, 255, 50, SWOOSH_HIGHLIGHT);

      byte fadeMax = map(revealTimer.getRemaining(), 0, FADE_DURATION, 0, 255);

      FOREACH_FACE(f) {
        byte swooshLevel = max(fadeMax, (highlightMax - map((millis() + ((SWOOSH_PERIOD / 6) * f)) % SWOOSH_PERIOD, 0, SWOOSH_PERIOD, 0, highlightMax)));
        if (f == orientationFace) {
          setColorOnFace(dim(WHITE, swooshLevel), f);
        } else {
          setColorOnFace(dim(teamColors[teamColor], swooshLevel), f);
        }
      }
    } else {//revealed display
      //so we have a default visual
      setColor(OFF);
      setColorOnFace(teamColors[teamColor], 0);
      if (currentBid > 1) {
        setColorOnFace(teamColors[teamColor], 1);
      }
      if (currentBid > 2) {
        setColorOnFace(teamColors[teamColor], 5);
      }
    }
  } else if (resultsTimer.getRemaining() > (RESULTS_2 + RESULTS_3 + RESULTS_4)) {//stage 1
    setColor(OFF);
    setColorOnFace(dim(teamColors[teamColor], 100), random(5));
  } else {//stages 2+3+4
    //display bid in dim color, unless you're the winner
    setColor(OFF);
    setColorOnFace(teamColors[teamColor], resultsMem);
    if (currentBid > 1) {
      setColorOnFace(teamColors[teamColor], (resultsMem + 1) % 6);
    }
    if (currentBid > 2) {
      setColorOnFace(teamColors[teamColor], (resultsMem + 5) % 6);
    }
  }
}

#define DIAMOND_HUE 175
#define DIAMOND_SAT_MAX 100

Timer sparkleTimer;
byte sparkleFace = 0;

void diamondDisplay() {
  if (resultsTimer.isExpired()) {//normal display
    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 255));

    if (sparkleTimer.isExpired()) {
      sparkleTimer.set(1000);
      sparkleFace = random(5);
    }

    if (sparkleTimer.getRemaining() < 250) {//do a little fade down
      byte sat = 100 - map(sparkleTimer.getRemaining(), 0, 250, 0, DIAMOND_SAT_MAX);
      setColorOnFace(makeColorHSB(DIAMOND_HUE, sat, 255), sparkleFace);
    }
  } else if (resultsTimer.getRemaining() > (RESULTS_2 + RESULTS_3 + RESULTS_4)) {//stage 1

    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));
    setColorOnFace(WHITE, random(5));

  } else if (resultsTimer.getRemaining() > (RESULTS_4)) {//stage 2 and 3

    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));
    setColorOnFace(dim(WHITE, 100), random(5));
    if (resultsMem < 6) {
      setColorOnFace(WHITE, resultsMem);
    }
  } else {//stage 4
    byte fadeMap = 255 - map(resultsTimer.getRemaining(), 0, RESULTS_4, 0, 155);
    setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, fadeMap));
  }
}

void conduitDisplay() {
  setColor(makeColorHSB(DIAMOND_HUE, DIAMOND_SAT_MAX, 100));

  if (sparkleTimer.isExpired()) {
    sparkleTimer.set(1000);
    sparkleFace = random(5);
  }

  if (sparkleTimer.getRemaining() < 250) {//do a little fade down
    byte sat = 100 - map(sparkleTimer.getRemaining(), 0, 250, 0, DIAMOND_SAT_MAX);
    setColorOnFace(makeColorHSB(DIAMOND_HUE, sat, 100), sparkleFace);
  }

  switch (pointsEarned) {
    case 5:
      setColorOnFace(teamColors[teamColor], (orientationFace + 4) % 6);
    case 4:
      setColorOnFace(teamColors[teamColor], (orientationFace + 2) % 6);
    case 3:
      setColorOnFace(teamColors[teamColor], (orientationFace + 5) % 6);
    case 2:
      setColorOnFace(teamColors[teamColor], (orientationFace + 1) % 6);
    case 1:
      setColorOnFace(teamColors[teamColor], orientationFace);
      break;
  }
}

void resetDisplay() {
  if (blinkState == RESET_ALL) {
    setColor(YELLOW);
  } else {
    setColor(dim(YELLOW, 100));
  }
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
