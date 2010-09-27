//////////////////////////////////////////////////////////////////////////////
// Needleman_Wunsch implementation, for pairwise global alignment
//
// Written by Rong She (rshe@bcgsc.ca)
// Last modified: Jul 30, 2010
//////////////////////////////////////////////////////////////////////////////

#include "needleman_wunsch.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

#ifndef __GNUC__
#ifndef NGNUC_INDEX
#define NGNUC_INDEX
#define INDEX_ONE_DIM(num_rows, row, col) (row * num_rows + col)
#endif
#endif

static bool Match(char a, char b, char& consensus);

template <class T>
static T find_array_max(T array[],int length, int& ind);

template <class T>
static void AssignScores_std(T* temp, T H_im1_jm1, T H_im1_j, T H_i_jm1, const char seq_a_im1, const char seq_b_jm1);

template <class T>
static void AssignScores_scorematrix(T* temp, T H_im1_jm1, T H_im1_j, T H_i_jm1, char seq_a_im1, char seq_b_jm1,
	bool& opengap_im1_jm1, bool& opengap_im1_j, bool& opengap_i_jm1, bool* temp_opengap);

/** A character representing a gap. */
static const char GAP = '*';

//the backtrack step
#ifdef __GNUC__
static unsigned Backtrack(const int i_max, const int j_max, int** I_i, int** I_j,
		const string& seq_a, const string& seq_b, NWAlignment& align)
#else
static unsigned Backtrack(const int i_max, const int j_max, int* I_i, int* I_j, const int I_i_rows, const int I_j_rows,
		const string& seq_a, const string& seq_b, NWAlignment& align)
#endif
{
	// Backtracking from H_max
	int current_i=i_max,current_j=j_max;
#ifdef __GNUC__
	int next_i=I_i[current_i][current_j];
	int next_j=I_j[current_i][current_j];
#else
	int next_i=I_i[INDEX_ONE_DIM(I_i_rows, current_i, current_j)];
	int next_j=I_j[INDEX_ONE_DIM(I_j_rows, current_i, current_j)];
#endif
	string consensus_a(""), consensus_b(""), match("");
	unsigned num_of_match = 0;
	//while(((current_i!=next_i) || (current_j!=next_j)) && (next_j!=0) && (next_i!=0)){
	while (current_i > 0 && current_j > 0) {
		if(next_i==current_i) {
			consensus_a += GAP; //deletion in A
			consensus_b += seq_b[current_j-1]; //b must be some actual char, cannot be GAP aligns with GAP!
		}
		else {
			consensus_a += seq_a[current_i-1]; // match/mismatch in A
			if(next_j==current_j)
				consensus_b += GAP; // deletion in B
			else
				consensus_b += seq_b[current_j-1]; // match/mismatch in B
		}
		char consensus_char;
		if (Match(*(consensus_a.rbegin()), *(consensus_b.rbegin()), consensus_char))
			num_of_match++;
		match += consensus_char; //seq_a[current_i-1];

		current_i = next_i;
		current_j = next_j;
#ifdef __GNUC__
		next_i=I_i[current_i][current_j];
		next_j=I_j[current_i][current_j];
#else
		next_i = I_i[INDEX_ONE_DIM(I_i_rows, current_i, current_j)];
		next_j = I_j[INDEX_ONE_DIM(I_j_rows, current_i, current_j)];
#endif
	}

	//fill the rest
	while (current_i > 0) {
		consensus_a += seq_a[current_i-1];
		consensus_b += GAP;
		char consensus_char;
		Match(seq_a[current_i-1], GAP, consensus_char);
		match += consensus_char;
		current_i--;
	}

	while (current_j > 0) {
		consensus_a += GAP;
		consensus_b += seq_b[current_j-1];
		char consensus_char;
		Match(GAP, seq_b[current_j-1], consensus_char);
		match += consensus_char;
		current_j--;
	}

	reverse(consensus_a.begin(), consensus_a.end());
	reverse(consensus_b.begin(), consensus_b.end());
	reverse(match.begin(), match.end());
	align.query_align = consensus_a;
	align.match_align = match;
	align.target_align = consensus_b;

	return num_of_match;
}

//This is the Needleman-Wunsch algirthm, returns one (first) optimal global alignment
//the alignment is stored in "align" (match_align is the consensus)
//return the number of matches in alignment (for calcing PID)
unsigned GetGlobalAlignment(const string& seq_a, const string& seq_b,
	NWAlignment& align, bool verbose)
{
	// get the actual lengths of the sequences
	int N_a = seq_a.length();
	int N_b = seq_b.length();

	// initialize H
	int i, j;
	double** H;
	int **I_i, **I_j;
	H = new double*[N_a+1];
	I_i = new int*[N_a+1];
	I_j = new int*[N_a+1];

	//bool **opengap; //use this to keep track whether a gap is opengap
	//opengap = new bool*[N_a+1];

	for(i=0;i<=N_a;i++){
		H[i] = new double[N_b+1];
		I_i[i] = new int[N_b+1];
		I_j[i] = new int[N_b+1];
		//opengap[i] = new bool[N_b+1];
		H[i][0]=0; //only need to initialize first row and first column
		//opengap[i][0] = false;
	}

	for (j=0;j<=N_b;j++){
		H[0][j]=0; //initialize first column
		//opengap[0][j] = false;
	}

	double temp[3];
	//bool temp_opengap[3];
	int ind;

	// here comes the actual algorithm
	for(i=1;i<=N_a;i++){
		for(j=1;j<=N_b;j++){
			AssignScores_std<double>(temp, H[i-1][j-1], H[i-1][j], H[i][j-1], seq_a[i-1], seq_b[j-1]);
			/*AssignScores_scorematrix<double>(temp, H[i-1][j-1], H[i-1][j], H[i][j-1], seq_a[i-1], seq_b[j-1],
				opengap[i-1][j-1], opengap[i-1][j], opengap[i][j-1], temp_opengap);*/

			H[i][j] = find_array_max<double>(temp, 3, ind);
			switch(ind){
			case 0: // score in (i,j) stems from a match/mismatch
				I_i[i][j] = i-1;
				I_j[i][j] = j-1;
				//opengap[i][j] = temp_opengap[0];
				break;
			case 1: // score in (i,j) stems from a deletion in sequence A
				I_i[i][j] = i-1;
				I_j[i][j] = j;
				//opengap[i][j] = temp_opengap[1];
				break;
			case 2: // score in (i,j) stems from a deletion in sequence B
				I_i[i][j] = i;
				I_j[i][j] = j-1;
				//opengap[i][j] = temp_opengap[2];
				break;
			}
		}
	}

	// search H for the maximal score
	unsigned num_of_match = 0;
	int i_max=N_a, j_max=N_b;
	//double H_max = H[i_max][j_max];
#ifdef __GNUC__
	num_of_match = Backtrack(i_max, j_max, I_i, I_j, seq_a, seq_b, align);
#else
	num_of_match = Backtrack(i_max, j_max, I_i, I_j, N_a+1, N_a+1,
					seq_a, seq_b, align);
#endif
	if (verbose)
		cerr <<"The alignment of the sequences\n" << seq_a << endl << seq_b << endl
			<< align << (double)num_of_match/align.size() << endl;

	//clean up memory
	for(i=0;i<=N_a;i++){
		delete [] H[i];
		delete [] I_i[i];
		delete [] I_j[i];
		//delete [] opengap[i];
	}
	delete [] H;
	delete [] I_i;
	delete [] I_j;
	//delete [] opengap;

	return num_of_match;
} // END of needleman-wunsch

/////////////////////////////////////////////////////////////////////////////
// auxiliary functions:
/////////////////////////////////////////////////////////////////////////////

static map<string, char> initIUPAC()
{
	typedef map<string, char> Map;
	Map iupac;
	iupac.insert(Map::value_type("AG", 'R'));
	iupac.insert(Map::value_type("CT", 'Y'));
	iupac.insert(Map::value_type("AC", 'M'));
	iupac.insert(Map::value_type("GT", 'K'));
	iupac.insert(Map::value_type("CG", 'S'));
	iupac.insert(Map::value_type("AT", 'W'));
	iupac.insert(Map::value_type("ACT", 'H'));
	iupac.insert(Map::value_type("CGT", 'B'));
	iupac.insert(Map::value_type("ACG", 'V'));
	iupac.insert(Map::value_type("AGT", 'D'));
	//also reverse order for pairwise consensus
	iupac.insert(Map::value_type("GA", 'R'));
	iupac.insert(Map::value_type("TC", 'Y'));
	iupac.insert(Map::value_type("CA", 'M'));
	iupac.insert(Map::value_type("TG", 'K'));
	iupac.insert(Map::value_type("GC", 'S'));
	iupac.insert(Map::value_type("TA", 'W'));
	return iupac;
}

/** Return whether a and b match.
 * @param [out] consensus the consensus of a and b
 */
static bool Match(char a, char b, char& consensus)
{
	assert(a != GAP || b != GAP);
	if (a == b) {
		consensus = a;
		return true;
	} else if (toupper(a) == toupper(b)) {
		consensus = islower(a) || islower(b) ? tolower(a) : a;
		return true;
	} else if (a == GAP) {
		consensus = tolower(b);
		return false;
	} else if (b == GAP) {
		consensus = tolower(a);
		return false;
	} else if (a == 'N') {
		consensus = b;
		return true;
	} else if (b == 'N') {
		consensus = a;
		return true;
	} else { //mismatch
		static map<string, char> IUPAC_codes = initIUPAC();
		string str(1, a);
		str += b;
		consensus = (IUPAC_codes.find(str))->second;
		return false;
	}
}

// The simple score matrix, match:1, mismatch/gap:-1
static int sim_score(const char a, const char b)
{
	char consensus;
	if (Match(a, b, consensus))
		return 1;
	else
		if (consensus == GAP)
			return -2; //gap penalty
		else
			return -1; //mismatch penalty
}

/*
// find the index of 'a' in the amino-acid substitution matrix, a is (a-z)|(A-Z)|-|*
int score_index(char a)
{
  if (a <= 'Z' && a >= 'A')
	  return a - 'A';
  else
	  if (a <= 'z' && a >= 'a')
		return a - 'a';
	  else
		if (a == '-')
			return 26;
		else
		{
			if (a == '*')
				return 27;
			else
			{
				cout << "cannot find score when running SW local alignment: " << "char:" << a << endl;
				exit(-1);
			}
		}
}

// find the score of (a,b) using amino acid substitution matrix, also update opengap status
int similarity_score(char a,char b, bool opengap, bool& opengap_cur)
{
  //ALIGN_SCORE_MATRIX[28][28] stores the align scores, 26 Uppercase letters (a.a.), '-', '*'
  int ai = score_index(a);
  int bi = score_index(b);
  if (ai != 26 && bi != 26)
  {
	  opengap_cur = false;
	  return ALIGN_SCORE_MATRIX[ai][bi];
  }
  else
  {
	  opengap_cur = true;
	  if (opengap) //previous is gap
		  return ALIGN_SCORE_MATRIX[26][1]; //gap extension
	  else
		  return ALIGN_SCORE_MATRIX[26][0]; //gap opening
  }
}
*/

// Find the max value in array[], use 'ind' to hold the index of max
template <class T>
static T find_array_max(T array[],int length, int& ind)
{

  T max = array[0];            // start with max = first element
  ind = 0;

  for(int i = 1; i<length; i++){
      if(array[i] > max){
		max = array[i];
		ind = i;
      }
  }
  return max;                    // return highest value in array
}

//compute alignment score based on simple score matrix: match is 1, mismatch/deletion/insertion is -1
template <class T>
static void AssignScores_std(T* temp, T H_im1_jm1, T H_im1_j, T H_i_jm1, const char seq_a_im1, const char seq_b_jm1)
{
  temp[0] = H_im1_jm1+sim_score(seq_a_im1,seq_b_jm1);
  temp[1] = H_im1_j+sim_score(seq_b_jm1, GAP);
  temp[2] = H_i_jm1+sim_score(seq_a_im1, GAP);
}

/*
//compute alignment score based on a substitution score matrix
template <class T>
void AssignScores_scorematrix(T* temp, T H_im1_jm1, T H_im1_j, T H_i_jm1, char seq_a_im1, char seq_b_jm1,
	bool& opengap_im1_jm1, bool& opengap_im1_j, bool& opengap_i_jm1, bool* temp_opengap)
{
	temp[0] = H_im1_jm1+similarity_score(seq_a_im1,seq_b_jm1, opengap_im1_jm1, temp_opengap[0]);
	temp[1] = H_im1_j+similarity_score(seq_a_im1, GAP, opengap_im1_j, temp_opengap[1]);
	temp[2] = H_i_jm1+similarity_score(seq_b_jm1, GAP, opengap_i_jm1, temp_opengap[2]);
	//temp[3] = 0.;
}
*/

//////////////////////////////////////////////////////////////////////////////

