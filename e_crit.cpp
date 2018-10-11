#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

struct termios orig_termios;
int screenRows = 0;
int screenColumns = 0;
std::string screenText(" ");
std::string textPile;
std::vector<int> whichRowAreWeIn;
std::vector<std::string> rows;
std::vector<std::vector<int>> whichColumnAreWeIn;
std::vector<std::vector<int>> whereInTheTextPileIsThisColumn;
int cursorRow = 1;
int cursorColumn = 0;
int cursorTextPileLocation = 0;
int row_pos_of_cursor = 0;
int lineh = 0;
int linev = 0;

bool savedState = true;

std::string p_phrase;

#define CTRL_KEY(k) ((k) & 0x1f)

void disableRawMode(){
  if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios) == -1);
}

void enableRawMode(){
  tcgetattr(STDIN_FILENO,&orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void getPassPhrase(){
  bool cont = true;
  struct termios getBackToThis;
  tcgetattr(STDIN_FILENO,&getBackToThis);

  struct termios pp = getBackToThis;

  pp.c_lflag &= ~(ECHO);
  tcsetattr(STDIN_FILENO,TCSAFLUSH,&pp);
  std::string temp("");
  while(cont){
    printf("enter password(shouldn't contain whitespaces):");
    std::cin >>p_phrase;
    printf("enter again:");
    std::cin >> temp;
    if(p_phrase.compare(temp)!=0){
      printf("\nthe passwords didn't match.\n");
    }
    else cont = 0;
  }
  tcsetattr(STDIN_FILENO,TCSAFLUSH,&getBackToThis);

}

void encrypt(){
  for(int i = 0; i< textPile.size();i++){
    textPile[i] = (char)(((int)textPile[i] + (int)p_phrase[i%p_phrase.size()])%128);
  }
}

void decrypt(){
  for(int i = 0; i< textPile.size();i++){
    textPile[i] = (char)(((int)textPile[i] - (int)p_phrase[i%p_phrase.size()] + 256)%128);
  }
}
void decideRows(){
  int rp = 1;
  int cp = 1;
  int _lineh = 0;
  int _linev = 0;
  rows.clear();
  whichRowAreWeIn.clear();
  whichColumnAreWeIn.clear();
  whereInTheTextPileIsThisColumn.clear();
  whichRowAreWeIn.push_back(1);
  bool overflowFlag = false;
  int no_spaces;
  cp++;
  rows.push_back(std::string("~"));
  whichColumnAreWeIn.push_back(std::vector<int>());
  whereInTheTextPileIsThisColumn.push_back(std::vector<int>());
  whichColumnAreWeIn[0].push_back(1);
  whereInTheTextPileIsThisColumn[0].push_back(0);
  row_pos_of_cursor = rp;
  cursorColumn = cp;
  for(int i = 0; i < textPile.size(); i++){
    if(textPile[i] == '\n' | textPile[i] == '\r'){
      rp ++;
      cp = 1;
      overflowFlag = false;
      _linev ++;
      _lineh = 1;
    }
    else if(textPile[i] == '\t'){
      no_spaces = std::min(4 - ((cp-2)%4),screenColumns-cp+1);
      rows[rows.size()-1].append(no_spaces,' ');
      cp += no_spaces;
      if(cp > screenColumns){
        rp++;
        cp = 1;
        overflowFlag = true;
      }
      _lineh ++;
    }
    else if(textPile[i] != 27){
      rows[rows.size() -1].append(1,textPile[i]);
      cp ++;
      if(cp > screenColumns){
        rp++;
        cp = 1;
        overflowFlag = true;
      }
      _lineh ++;
    }
    if(cp == 1){
      cp++;
      rows.push_back(std::string());
      whichColumnAreWeIn.push_back(std::vector<int>());
      whereInTheTextPileIsThisColumn.push_back(std::vector<int>());
      if(overflowFlag){
        rows[rows.size()-1].append(" ");
      }
      else{
        rows[rows.size()-1].append("~");
      }
    }
    if(i == cursorTextPileLocation-1){
      row_pos_of_cursor = rp;
      cursorColumn = cp;
      linev = _linev;
      lineh = _lineh;
    }
    whichRowAreWeIn.push_back(rp);
    whichColumnAreWeIn[whichColumnAreWeIn.size()-1].push_back(cp);
    whereInTheTextPileIsThisColumn[whereInTheTextPileIsThisColumn.size()-1].push_back(i+1);
  }
  if(cp == 1){
    cp++;
    rows.push_back(std::string());
    if(overflowFlag){
      rows[rows.size()-1].append(" ");
    }
    else{
      rows[rows.size()-1].append("~");
    }
    cursorColumn ++;
  }
}

void decideScreenText(){
  if(cursorRow >= screenRows){
    cursorRow = screenRows/2;
  }
  if( cursorRow > (row_pos_of_cursor+1) ){
    cursorRow = 1;
  }
  int startRow = row_pos_of_cursor - cursorRow;
  int endRow = std::min(startRow + screenRows - 1,(int)rows.size());
  screenText.clear();
  for (int i = startRow; i < endRow; i++){
    screenText.append(rows[i]);
    if(i != endRow-1){
      screenText.append("\r\n");
    }
  }
}

void printStatus(){
  write(STDOUT_FILENO,"\x1b[1;1H",6);
  std::string str = std::string("\x1b[1C\x1b[").append(std::to_string(screenRows)).append("B\x1b[2K");
  write(STDOUT_FILENO,str.c_str(),str.size());
  str = std::string("location: ").append(std::to_string(linev)).append("/").append(std::to_string(lineh));
  if(!savedState) str.append("*");
  write(STDOUT_FILENO,str.c_str(),str.size());
}

void place_cursor(){
  std::string place_cursor_string = std::string("\x1b[").append(std::to_string(cursorRow)).append(";").append(std::to_string(cursorColumn)).append("H");
  write(STDOUT_FILENO,place_cursor_string.c_str(),place_cursor_string.size());
}

void drawScreen(){
  write(STDOUT_FILENO,"\x1b[H\x1b[0J",7);
  write(STDOUT_FILENO,screenText.c_str(),screenText.size());
  printStatus();
  place_cursor();
}

bool check_for_resize(){
  struct winsize ws;
  ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws);
  if(ws.ws_row != screenRows | ws.ws_col != screenColumns){
    screenRows = ws.ws_row;
    screenColumns = ws.ws_col;
    return true;
  }
  else return false;
}

bool resize_actions(){
  decideRows();
  decideScreenText();
  drawScreen();
}

void action(char c){
  int init_row = whichRowAreWeIn[cursorTextPileLocation];
  if(c == 0){
    return;
  }
  else if(c == 127 | c == 8){
    if(cursorTextPileLocation>0){
      textPile.erase(cursorTextPileLocation-1,1);
      cursorTextPileLocation --;
      decideRows();
      if(whichRowAreWeIn[cursorTextPileLocation] < init_row){
        cursorRow = std::max(1,cursorRow-1);
      }
      decideScreenText();
      drawScreen();
    }
  }
  else if(c == CTRL_KEY('f')){
    if(cursorTextPileLocation < textPile.size()){
      cursorTextPileLocation ++;
      decideRows();
      if(whichRowAreWeIn[cursorTextPileLocation] > init_row){
        cursorRow ++;
      }
      decideScreenText();
      drawScreen();
    }
  }
  else if(c == CTRL_KEY('b')){
    if(cursorTextPileLocation > 0){
      cursorTextPileLocation --;
      decideRows();
      if(whichRowAreWeIn[cursorTextPileLocation] < init_row){
        cursorRow = std::max(1,cursorRow-1);
      }
      decideScreenText();
      drawScreen();
    }
  }
  else if(c == CTRL_KEY('p')){
    if(row_pos_of_cursor > 1){
      int new_text_pile_loc = whereInTheTextPileIsThisColumn[row_pos_of_cursor-2][0];
      for (int i = 0; i < whereInTheTextPileIsThisColumn[row_pos_of_cursor-2].size(); i++){
        if(whichColumnAreWeIn[row_pos_of_cursor-2][i]>cursorColumn){
          break;
        }
        else{
          new_text_pile_loc = whereInTheTextPileIsThisColumn[row_pos_of_cursor - 2][i];
        }
      }
      cursorTextPileLocation = new_text_pile_loc;
      cursorRow = std::max(1,cursorRow - 1);
    }
    else cursorTextPileLocation = 0;
    decideRows();
    decideScreenText();
    drawScreen();
  }
  else if(c == CTRL_KEY('n')){
    if(row_pos_of_cursor < rows.size()){
      int new_text_pile_loc = whereInTheTextPileIsThisColumn[row_pos_of_cursor][0];
      for (int i = 0; i < whereInTheTextPileIsThisColumn[row_pos_of_cursor].size(); i++){
        if(whichColumnAreWeIn[row_pos_of_cursor][i]>cursorColumn){
          break;
        }
        else{
          new_text_pile_loc = whereInTheTextPileIsThisColumn[row_pos_of_cursor][i];
        }
      }
      cursorTextPileLocation = new_text_pile_loc;
      cursorRow = std::min(screenRows-1,cursorRow + 1);
    }
    else cursorTextPileLocation = textPile.size();
    decideRows();
    decideScreenText();
    drawScreen();
  }
  else if(c > 31 | c == '\n' | c == '\r' | c == '\t'){
    textPile.insert(cursorTextPileLocation,1,c);
    cursorTextPileLocation ++;
    decideRows();
    if(whichRowAreWeIn[cursorTextPileLocation] > init_row){
      cursorRow ++;
    }
    savedState = false;
    decideScreenText();
    drawScreen();
  }
}

int main(int argc, char** argv){
  if(argc != 2){
    printf("provide filename");
    exit(1);
  }
  getPassPhrase();
  std::fstream file;
  file.open(argv[1]);
  char u;
  while(file.get(u)){
    textPile.append(1,u);
  }
  file.close();
  decrypt();
  /*std::string ntp;
  for(int i = 0; i < textPile.size(); i++){
    ntp.append(std::to_string(textPile[i]));
    ntp.append(" ");
  }
  textPile = ntp;*/
  enableRawMode();
  char c;
  bool resized;
  while( c != CTRL_KEY('q')){
    c = 0;
    read(STDIN_FILENO, &c,1);
    resized = check_for_resize();
    if(resized) resize_actions();
    action(c);
    if(c == CTRL_KEY('s')){
      encrypt();
      std::ofstream outfile;
      outfile.open(argv[1]);
      outfile.write(textPile.c_str(),(int)textPile.size());
      outfile.close();
      savedState = true;
      drawScreen();
      decrypt();
    }
  }
  write(STDOUT_FILENO,"\x1b[H\x1b[0J",7);
  disableRawMode();
}
