bool isTreasure = false;
bool isDiamond = false;

Color teamColors[6] = {RED, ORANGE, YELLOW, GREEN, CYAN, MAGENTA};
byte teamColor = 1;
bool isRevealed = false;
byte currentBid = 1;

byte pointsEarned = 0;
bool pointsPassed = false;
byte diamondFace = 6;
byte diamondSignal;
byte banditFace = 6;
byte banditSignal;

bool showingResults = false;
Timer resultTimer;
#define RESULTS_INTERVAL 1000

byte prizeSignal = 0;
byte winningFace = 6;

Timer revealTimer;
#define REVEAL_INTERVAL 1000
#define REVEAL_FADE 500

void setup() {

}

void loop() {

  //do loops
  if (isTreasure) {
    treasureLoop();
  } else {
    banditLoop();
  }

  //do display
  if (isTreasure) {
    if (isDiamond) {
      setColor (WHITE);
      if (winningFace != 6) {
        setColorOnFace(RED, winningFace);
      }
    } else {
      setColor(OFF);
      FOREACH_FACE(f) {
        if (f < pointsEarned) {
          setColorOnFace(WHITE, f);
        }
      }
    }
  } else {
    if (isRevealed) {
      setColor(OFF);
      FOREACH_FACE(f) {
        if (f < currentBid) {
          setColorOnFace(teamColors[teamColor], f);
        }
      }
    } else {
      FOREACH_FACE(f) {
        setColorOnFace(dim(teamColors[teamColor], random(150)), f);
      }
    }
  }

  //do communication

  if (isTreasure) {
    if (isDiamond) {
      if (showingResults) {
        FOREACH_FACE(f) {
          byte sendData;
          if (winningFace == f) {
            sendData = (isTreasure << 5) + (isDiamond << 4) + (showingResults << 3) + prizeSignal;
          } else {
            sendData = (isTreasure << 5) + (isDiamond << 4) + (showingResults << 3);
          }
          setValueSentOnFace(sendData, f);
        }
      } else {
        FOREACH_FACE(f) {
          byte sendData = (isTreasure << 5) + (isDiamond << 4) + (showingResults << 3);
          setValueSentOnFace(sendData, f);
        }
      }
    } else {//conduit signaling
      setValueSentOnAllFaces(0);
      if (diamondFace != 6) {//this is the special conduit mirroring
        setValueSentOnFace(diamondSignal, banditFace);
        setValueSentOnFace(banditSignal, diamondFace);
      }
    }
  } else {
    FOREACH_FACE(f) {
      byte sendData = (isTreasure << 5) + (isDiamond << 4) + (currentBid);
      setValueSentOnFace(sendData, f);
    }
  }


  //dump button presses
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonMultiClicked();
}

void treasureLoop() {

  if (buttonMultiClicked()) {
    isTreasure = false;
    isDiamond = false;
  }

  if (isDiamond) {
    diamondLoop();
  } else {
    conduitLoop();
  }
}

void diamondLoop() {
  if (showingResults) {
    if (resultTimer.isExpired()) {
      showingResults = false;
    }
  } else {
    //so we need to look around for the bids and determine a winner
    //here's what I want to know
    byte bidCount[3] = {0, 0, 0};//this tells me how many bids of 1/2/3 we receive
    byte bidLocation[3] = {6, 6, 6};//this tell me where the bid is located, defaulting to 6 because that's nowhere

    //run through the faces and fill out these arrays
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getIsTreasure(neighborData) == false) {
          byte thisBid = getBid(neighborData);
          bidCount[thisBid - 1] += 1;//increment the count for this type of bid
          bidLocation[thisBid - 1] = f;//set this as the location for that bid. Overwriting is fine because duplicates can't score anyway
        }
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

    if (buttonDoubleClicked()) {//ok, so this is where we do the reveal
      showingResults = true;
      resultTimer.set(RESULTS_INTERVAL);
    }


  }


}

void conduitLoop() {

  //first step, determine where the diamond is
  diamondFace = 6;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getIsTreasure(neighborData) == true && getIsDiamond(neighborData) == true) {//hey, a diamond!
        diamondFace = f;
        diamondSignal = (neighborData & 56) + prizeSignal;
      }
    }
  }

  //now, extrapolate where the potential bandit face should be
  banditFace = 6;
  if (diamondFace != 6) {
    banditFace = (diamondFace + 3) % 6;
    if (!isValueReceivedOnFaceExpired(banditFace)) {//oh, I've got a neighbor there
      byte neighborData = getLastValueReceivedOnFace(banditFace);
      if (getIsTreasure(neighborData) == false) {//hey, it's a bandit!
        banditSignal = neighborData;
      } else {
        banditSignal = 0;
      }
    }  else {
      banditSignal = 0;
    }
  }
}

void banditLoop() {
  if (buttonMultiClicked()) {
    isTreasure = true;
    isDiamond = true;
  }

  //single click cycles through bids
  if (buttonSingleClicked()) {
    //if I'm currently visible, increment count. Otherwise, just become revealed
    if (isRevealed) {
      currentBid += 1;
      if (currentBid > 3) {
        currentBid = 1;
      }
    }
    revealTimer.set(REVEAL_INTERVAL + REVEAL_FADE);
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

      if (getIsTreasure(neighborData) == true && getIsDiamond(neighborData) == true) {//this is my diamond neighbor (or the one faking it)!
        //is it telling me to show results, and if so, did I win a prize?
        if (getShowingResults(neighborData) == true) {//time to show results!
          if (getPrizeSignal(neighborData) > 0) {//ooh, we won a prize
            //this is where we gotta do a lot. This is where we become... a conduit!
            isTreasure = true;
            pointsEarned = getPrizeSignal(neighborData);
            pointsPassed = false;
            prizeSignal = pointsEarned - (pointsEarned / 2);
          }
        }
      }
    }
  }
}

bool getIsTreasure(byte data) {
  return (data >> 5);
}

bool getIsDiamond(byte data) {
  return ((data >> 4) & 1);
}

byte getBid(byte data) {
  return (data & 3);
}

bool getShowingResults (byte data) {
  return ((data >> 3) & 1);
}

byte getPrizeSignal (byte data) {
  return (data & 7);
}
