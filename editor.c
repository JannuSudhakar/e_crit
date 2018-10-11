/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/***defines***/
#define CTRL_KEY(k) ((k) & 0x1f)

/***utils***/
int no_rows_str_occupies(std::string str,int row_width){
  /*row_width is exclusive of any tildae I might decide to print at the start
  of each line, that has to be subtaracted out before passing it here. Tabs
  are considered as 4 spaces and will end when they hit the end of a line. Also
  tabs will stop when they hit multiples of 4 on the row (as is normal). Also,
  non alphanumeric/whitespace characters are attempted for display.*/
  int returnValue = 0;
  int curr_col = 0;
  for(int i = 0; i < str.size(); i++){
    if(str[i] == '\t'){
      curr_col = (curr_col+4)/4;
    }
    else if(str[i] == ' '){
      curr_col += 1;
    }
    else if(str[i] == '\n'){
      returnValue += 1;
      curr_col = 0;
    }
    else if(str[i] == '\x1b'){
      //do nothing
    }
    else{
      curr_col += 1;
    }
    if(curr_col >= row_width-1){
      returnValue += 1;
      curr_col = 0;
    }
  }
  return returnValue+1;
}

/***environment variables***/
std::string welcome_string("Welcome to the text editor, this is the temporary welcome message which will eventually be replaced with details of how to operate the editor. For now this body of text will act as the test text that will be edited and played around with for development purposes.\nLorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.");

/*** data ***/
struct cursor_pos{
  int row = 1;
  int column = 1;
  int bad_coding_flag = 0;
};

struct editorConfig{
  int screenRows = 0;
  int screenColumns = 0;

  int cursor_pos_in_text_pile = 15;
  int display_start_location_in_text_pile = 0;
  int curr_text_pile = 0;

  struct cursor_pos editor_current_cursor_position;

  std::vector<std::string> text_pile{welcome_string};

  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO,"\x1b[2J",4);
  write(STDOUT_FILENO,"\x1b[H",3);
  perror(s);
  exit(1);
}

void disableRawMode(){
  if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode(){
  if(tcgetattr(STDIN_FILENO,&E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int getWindowSize(){
  struct winsize ws;

  if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws) == 1 | ws.ws_col == 0){
    return -1;
  }
  else{
    int returnValue = 0;
    if(E.screenRows != ws.ws_row | E.screenColumns != ws.ws_col){
      returnValue = 1;
    }
    E.screenRows = ws.ws_row;
    E.screenColumns = ws.ws_col;
    return returnValue;
  }
}

/***output***/
void editorRefreshScreen(){
  write(STDOUT_FILENO,"\x1b[H",3);
  write(STDOUT_FILENO,"\x1b[0J",4);
}

void place_cursor(){
  std::string place_cursor_string = std::string("\x1b[").append(std::to_string(E.editor_current_cursor_position.row)).append(";").append(std::to_string(E.editor_current_cursor_position.column)).append("H");
  write(STDOUT_FILENO,place_cursor_string.c_str(),place_cursor_string.size());
}

struct cursor_pos print_char(char c,struct cursor_pos cp){
  struct cursor_pos out;
  if(c == '\n'){
    if(cp.bad_coding_flag == 0){
      write(STDOUT_FILENO,"\r\n",2);
      out.row = cp.row + 1;
    }
    else{
      out.row = cp.row;
      out.column = cp.column;
    }
  }
  else if(c=='\t'){
    int no_spaces = std::min(4 - (cp.column % 4),E.screenColumns - cp.column);
    write(STDOUT_FILENO,"    ",no_spaces);
    out.row = cp.row;
    out.column = cp.column + no_spaces;
  }
  else if(c == '\x1b' | c == '\t'){
  /*do nothing*/
    out.row = cp.row;
    out.column = cp.column;
  }
  else {
     write(STDOUT_FILENO,&c,1);
     out.row = cp.row;
     out.column = cp.column + 1;
   }
   if(out.column > E.screenColumns){
     out.row += 1;
     out.column = 1;
     out.bad_coding_flag = 1;
     write(STDOUT_FILENO,"\r\n",2);
   }
   return out;
}

void printStatus(){
  write(STDOUT_FILENO,"\x1b[1;1H",6);
  std::string str = std::string("\x1b[1C\x1b[").append(std::to_string(E.screenRows)).append("B\x1b[2K");
  write(STDOUT_FILENO,str.c_str(),str.size());
  str = std::string("win-size: ").append(std::to_string(E.editor_current_cursor_position.row)).append(" rows/ ").append(std::to_string(E.editor_current_cursor_position.column)).append(" columns.").append(std::to_string(E.cursor_pos_in_text_pile));
  write(STDOUT_FILENO,str.c_str(),str.size());
  place_cursor();
}

void drawTextPile(){
  write(STDOUT_FILENO,"\x1b[1;1H\x1b[0J",10);
  struct cursor_pos curr_cursor_pos;
  struct cursor_pos where_the_cursor_needs_to_be;
  where_the_cursor_needs_to_be.row = -1;
  where_the_cursor_needs_to_be.column = -1;
  int print_pos_in_text_pile = E.display_start_location_in_text_pile;
  write(STDOUT_FILENO,"~",1);
  curr_cursor_pos.column += 1;
  if(print_pos_in_text_pile == E.cursor_pos_in_text_pile) where_the_cursor_needs_to_be = curr_cursor_pos;
  while(curr_cursor_pos.row <= E.screenRows && print_pos_in_text_pile < E.text_pile[E.curr_text_pile].size()){
    if(curr_cursor_pos.column == 1){
      if(curr_cursor_pos.bad_coding_flag) write(STDOUT_FILENO," ",1);
      else {
        write(STDOUT_FILENO,"~",1);
      }
      curr_cursor_pos.column += 1;
    }
    if(print_pos_in_text_pile == E.cursor_pos_in_text_pile) where_the_cursor_needs_to_be = curr_cursor_pos;
    curr_cursor_pos = print_char(E.text_pile[E.curr_text_pile][print_pos_in_text_pile],curr_cursor_pos);
    print_pos_in_text_pile ++;
  }
  if(print_pos_in_text_pile == E.cursor_pos_in_text_pile) where_the_cursor_needs_to_be = curr_cursor_pos;
  E.editor_current_cursor_position = where_the_cursor_needs_to_be;
  place_cursor();
}

void resize_actions(){
  drawTextPile();
  printStatus();
}

/***input***/
int editorReadKey(char &c){
  int nread;
  int returnValue;
  while((nread = read(STDIN_FILENO, &c,1)) != 1){
    if(nread == -1 && errno != EAGAIN) die("read");
    if(getWindowSize() == 1){
      returnValue += 1;
      resize_actions();
    }
  }
  return returnValue;
}

int editorProcessKeypress(){
  char c;
  int resized = editorReadKey(c);
  //E.text_pile[0].append(std::to_string(E.cursor_pos_in_text_pile));
  //drawTextPile();
  //printStatus();
  switch(c){
    case CTRL_KEY('q'):
      return 0;
      break;
    case CTRL_KEY('f'):
      E.cursor_pos_in_text_pile ++;
      drawTextPile();
      printStatus();
      break;
    case CTRL_KEY('b'):
      E.cursor_pos_in_text_pile --;
      drawTextPile();
      printStatus();
      break;
    case CTRL_KEY('n'):
      E.cursor_pos_in_text_pile += (E.screenColumns-1);
      drawTextPile();
      printStatus();
      break;
    case CTRL_KEY('p'):
      E.cursor_pos_in_text_pile -= (E.screenColumns-1);
      drawTextPile();
      printStatus();
  }
  return 1;
}

/*** init ***/
int main(){
  enableRawMode();
  int cont = 1;
  while(cont){
    //editorRefreshScreen();
    cont = editorProcessKeypress();
  }
  editorRefreshScreen();
  return 0;
}
