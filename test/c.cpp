// Include the libraries
#include <iostream>

using namespace std;

int main(){
    // The game should proceed endlessly until the player 
    // wants to quit.
    while (true) {
        // When a new round begins,
        // first get a string "answer" from the file
        // and get a string "scrambled" from the file
        char answer[100];
        char scrambled[100];
        // Get the length of the answer.
        // Hint: use strlen
        int len; 
        // ...
        // Then initialize a partial solution array
        char partial[100];
        
        // The player has three chances
        for (int time = 0; time < 3; time ++) {
            
        }
        // Ask the player if the game should end
        // ...
    }
    return 0;
}