#include <TimerFactory.h>
//
// Created by bioinfo on 2/06/17.
//
#include "../include/utils.h"
#include "../include/values.h"

#include "../include/Cleaner.h"
#include "../include/Statistics/StatisticsManager.h"
#include "../include/newAlignment.h"
#include "../include/defines.h"

#include "../include/reportsystem.h"

#include <algorithm>
#include <queue>

int Cleaner::selectMethod(void) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("int Cleaner::selectMethod(void) ");

    float mx, avg, maxSeq = 0, avgSeq = 0;
    int i, j;

    // Ask for the sequence identities assesment
    if (_alignment->identities == NULL)
        calculateSeqIdentity();

    // Once we have the identities among all possible
    // combinations between each pair of sequence. We
    // compute the average identity as well as the
    // average identity for each sequence with its most
    // similar one
    for (i = 0; i < _alignment->sequenNumber; i++) {
        for (j = 0, mx = 0, avg = 0; j < _alignment->sequenNumber; j++) {
            if (i != j) {
                mx = mx < _alignment->identities[i][j] ? _alignment->identities[i][j] : mx;
                avg += _alignment->identities[i][j];
            }
        }
        avgSeq += avg / (_alignment->sequenNumber - 1);
        maxSeq += mx;
    }

    avgSeq = avgSeq / _alignment->sequenNumber;
    maxSeq = maxSeq / _alignment->sequenNumber;
    // With the different parameters, we decide wich one
    // is the best automated method, based on a previous
    // simulated data benchmarks, to trim the alig
    if (avgSeq >= 0.55) return GAPPYOUT;
    else if (avgSeq <= 0.38) return STRICT;

        // Sometimes we need to use more parameters to select
        // the best automated method, always based on our
        // benchmarks, to trim the input alignment
    else {
        if (_alignment->sequenNumber <= 20) return GAPPYOUT;
        else {
            if ((maxSeq >= 0.5) && (maxSeq <= 0.65)) return GAPPYOUT;
            else return STRICT;
        }
    }
}

newAlignment *Cleaner::cleanByCutValueOverpass(double cut, float baseLine, const int *gInCol, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanByCutValueOverpass(double cut, float baseLine, const int *gInCol, bool complementary) ");
    int i, j, k, jn, oth, pos, block, *vectAux, residues;
    newAlignment *newAlig = new newAlignment(*_alignment);

    // Select the columns with a gaps value
    // less or equal than the cut point.
    for (i = 0, residues = 0; i < _alignment->residNumber; i++) {
        if (gInCol[i] <= cut) residues++;
        else newAlig->saveResidues[i] = -1;

    }
    // Compute, if it's necessary, the number of columns
    // necessary to achieve the minimum number of columns
    // fixed by coverage parameter.
    oth = utils::roundInt((((baseLine / 100.0) - (float) residues / _alignment->residNumber)) * _alignment->residNumber);
    // if it's necessary to recover some columns,
    // we apply this instructions to recover it
    if (oth > 0) {
        // Allocate memory
        vectAux = new int[_alignment->residNumber];

        // Sort a copy of the gInCol vector, and take the value of the column that marks the % baseline
        utils::copyVect((int *) gInCol, vectAux, _alignment->residNumber);
        utils::quicksort(vectAux, 0, _alignment->residNumber - 1);
        cut = vectAux[(int) ((float) (_alignment->residNumber - 1) * (baseLine) / 100.0)];

        // Deallocate memory
        delete[] vectAux;
    }
    // Fixed the initial size of blocks as 0.5% of
    // alignment's length
    for (k = utils::roundInt(0.005 * _alignment->residNumber); (k >= 0) && (oth > 0); k--) {

        // We start in the alignment middle then we move on
        // right and left side at the same time.
        for (i = (_alignment->residNumber / 2), j = (i + 1); (((i > 0) || (j < (_alignment->residNumber - 1))) && (oth > 0)); i--, j++) {

            // Left side. Here, we compute the block's size.
            for (jn = i; ((newAlig->saveResidues[jn] != -1) && (jn >= 0) && (oth > 0)); jn--);

            // if block's size is greater or equal than the fixed
            // size then we save all columns that have not been
            // saved previously.
            if ((i - jn) >= k) {
                for (; ((newAlig->saveResidues[jn] == -1) && (jn >= 0) && (oth > 0)); jn--) {
                    if (gInCol[jn] <= cut) {
                        newAlig->saveResidues[jn] = jn;
                        oth--;
                    } else
                        break;
                }
            }
            i = jn;
            // Right side. Here, we compute the block's size.
            for (jn = j; ((newAlig->saveResidues[jn] != -1) && (jn < _alignment->residNumber) && (oth > 0)); jn++);

            // if block's size is greater or equal than the fixed
            // size then we save all columns that have not been
            // saved previously.
            if ((jn - j) >= k) {
                for (; ((newAlig->saveResidues[jn] == -1) && (jn < _alignment->residNumber) && (oth > 0)); jn++) {
                    if (gInCol[jn] <= cut) {
                        newAlig->saveResidues[jn] = jn;
                        oth--;
                    } else
                        break;
                }
            }
            j = jn;
        }
    }

    newAlig->Cleaning->removeSmallerBlocks(blockSize);

    // If the flag -terminalony is set, apply a method to look for internal
    // boundaries and get back columns inbetween them, if they exist
    if (terminalGapOnly && !removeOnlyTerminal()) return NULL;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active
    if (complementary) computeComplementaryAlig(true, false);

    newAlig->Cleaning->removeAllGapsSeqsAndCols();

    // Return the new alignment reference
    return newAlig;

}

newAlignment *Cleaner::cleanByCutValueFallBehind(float cut, float baseLine, const float *ValueVect, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanByCutValueFallBehind(float cut, float baseLine, const float *ValueVect, bool complementary) ");

    int i, j, k, jn, resCounter, NumberOfResiduesToAchieveBaseLine;
    newAlignment *newAlig = new newAlignment(*_alignment);

    for (i = 0, resCounter = 0; i < _alignment->originalResidNumber; i++)
    {
        if (ValueVect[i] > cut) resCounter++;
        else newAlig->saveResidues[i] = -1;
        debug << newAlig->saveResidues[i] << "\t" << ValueVect[i] << "\n";
    }

    NumberOfResiduesToAchieveBaseLine = utils::roundInt((((baseLine / 100.0) - (float) resCounter / _alignment->residNumber)) * _alignment->residNumber);


    // Fixed the initial size of blocks as 0.5% of
    // alignment's length
    for (k = utils::roundInt(0.005 * _alignment->residNumber); (k >= 0) && (NumberOfResiduesToAchieveBaseLine > 0); k--) {

        // We start in the alignment middle then we move on
        // right and left side at the same time.
        for (i = (_alignment->residNumber / 2), j = (i + 1); (((i > 0) || (j < (_alignment->residNumber - 1))) && (NumberOfResiduesToAchieveBaseLine > 0)); i--, j++) {

            // Left side. Here, we compute the block's size.
            for (jn = i; ((newAlig->saveResidues[jn] != -1) && (jn >= 0) && (NumberOfResiduesToAchieveBaseLine > 0)); jn--);

            // if block's size is greater or equal than the fixed
            // size then we save all columns that have not been
            // saved previously.
            if ((i - jn) >= k) {
                for (; ((newAlig->saveResidues[jn] == -1) && (jn >= 0) && (NumberOfResiduesToAchieveBaseLine > 0)); jn--) {
                    if (ValueVect[jn] == cut) {
                        newAlig->saveResidues[jn] = jn;
                        NumberOfResiduesToAchieveBaseLine--;
                    } else
                        break;
                }
            }
            i = jn;
            // Right side. Here, we compute the block's size.
            for (jn = j; ((newAlig->saveResidues[jn] != -1) && (jn < _alignment->residNumber) && (NumberOfResiduesToAchieveBaseLine > 0)); jn++);

            // if block's size is greater or equal than the fixed
            // size then we save all columns that have not been
            // saved previously.
            if ((jn - j) >= k) {
                for (; ((newAlig->saveResidues[jn] == -1) && (jn < _alignment->residNumber) && (NumberOfResiduesToAchieveBaseLine > 0)); jn++) {
                    if (ValueVect[jn] == cut) {
                        newAlig->saveResidues[jn] = jn;
                        NumberOfResiduesToAchieveBaseLine--;
                    } else
                        break;
                }
            }
            j = jn;
        }
    }

    newAlig->Cleaning->removeSmallerBlocks(blockSize);

    // If the flag -terminalony is set, apply a method to look for internal
    // boundaries and get back columns inbetween them, if they exist
    if (terminalGapOnly && !newAlig->Cleaning->removeOnlyTerminal())
            return NULL;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active
    if (complementary)
        newAlig->Cleaning->computeComplementaryAlig(true, false);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers 
    newAlig->Cleaning->removeAllGapsSeqsAndCols();
    return newAlig;
}

newAlignment *Cleaner::cleanByCutValueOverpassOrEquals(double cutGaps, const int *gInCol, float baseLine, float cutCons, const float *MDK_Win, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanByCutValueOverpassOrEquals(double cutGaps, const int *gInCol, float baseLine, float cutCons, const float *MDK_Win, bool complementary) ");

    int i, j, k, jn, resCounter, NumberOfResiduesToAchieveBaseLine, pos, block, *vectAux;
    newAlignment *newAlig = new newAlignment(*_alignment);

    for (i = 0, resCounter = 0; i < _alignment->originalResidNumber; i++)
        if ((MDK_Win[i] > cutCons) && (gInCol[i] <= cutGaps)) resCounter++;
        else newAlig->saveResidues[i] = -1;

    NumberOfResiduesToAchieveBaseLine = utils::roundInt((((baseLine / 100.0) - (float) resCounter / _alignment->residNumber)) * _alignment->residNumber);

    float blCons, *vectAuxCons;
    int blGaps, *vectAuxGaps;

    if (NumberOfResiduesToAchieveBaseLine > 0) {

        vectAuxCons = new float[_alignment->residNumber];
        utils::copyVect((float *) MDK_Win, vectAuxCons, _alignment->residNumber);
        utils::quicksort(vectAuxCons, 0, _alignment->residNumber - 1);
        blCons = vectAuxCons[(int) ((float) (_alignment->residNumber - 1) * (100.0 - baseLine) / 100.0)];
        delete[] vectAuxCons;

        vectAuxGaps = new int[_alignment->residNumber];
        utils::copyVect((int *) gInCol, vectAuxGaps, _alignment->residNumber);
        utils::quicksort(vectAuxGaps, 0, _alignment->residNumber - 1);
        blGaps = vectAuxGaps[(int) ((float) (_alignment->residNumber - 1) * (baseLine) / 100.0)];
        delete[] vectAuxGaps;

        // Fixed the initial size of blocks as 0.5% of
        // alignment's length
        for (k = utils::roundInt(0.005 * _alignment->residNumber); (k >= 0) && (NumberOfResiduesToAchieveBaseLine > 0); k--) {

            // We start in the alignment middle then we move on
            // right and left side at the same time.
            for (i = (_alignment->residNumber / 2), j = (i + 1); (((i > 0) || (j < (_alignment->residNumber - 1))) && (NumberOfResiduesToAchieveBaseLine > 0)); i--, j++) {

                // Left side. Here, we compute the block's size.
                for (jn = i; ((newAlig->saveResidues[jn] != -1) && (jn >= 0) && (NumberOfResiduesToAchieveBaseLine > 0)); jn--);

                // if block's size is greater or equal than the fixed
                // size then we save all columns that have not been
                // saved previously.
                if ((i - jn) >= k) {
                    for (; ((newAlig->saveResidues[jn] == -1) && (jn >= 0) && (NumberOfResiduesToAchieveBaseLine > 0)); jn--) {
                        if (MDK_Win[jn] >= blCons || gInCol[jn] <= blGaps) {
                            newAlig->saveResidues[jn] = jn;
                            NumberOfResiduesToAchieveBaseLine--;
                        } else
                            break;
                    }
                }
                i = jn;
                // Right side. Here, we compute the block's size.
                for (jn = j; ((newAlig->saveResidues[jn] != -1) && (jn < _alignment->residNumber) && (NumberOfResiduesToAchieveBaseLine > 0)); jn++);

                // if block's size is greater or equal than the fixed
                // size then we save all columns that have not been
                // saved previously.
                if ((jn - j) >= k) {
                    for (; ((newAlig->saveResidues[jn] == -1) && (jn < _alignment->residNumber) && (NumberOfResiduesToAchieveBaseLine > 0)); jn++) {
                        if (MDK_Win[jn] >= blCons || gInCol[jn] <= blGaps) {
                            newAlig->saveResidues[jn] = jn;
                            NumberOfResiduesToAchieveBaseLine--;
                        } else
                            break;
                    }
                }
                j = jn;
            }
        }
    }


    newAlig->Cleaning->removeSmallerBlocks(blockSize);

    // If the flag -terminalony is set, apply a method to look for internal
    // boundaries and get back columns inbetween them, if they exist 
    if (terminalGapOnly && !newAlig->Cleaning->removeOnlyTerminal())
            return NULL;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active 
    if (complementary)
        newAlig->Cleaning->computeComplementaryAlig(true, false);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers 
    newAlig->Cleaning->removeAllGapsSeqsAndCols(true, true);
    return newAlig;
}

newAlignment *Cleaner::cleanStrict(int gapCut, const int *gInCol, float simCut, const float *MDK_W, bool complementary, bool variable) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanStrict(int gapCut, const int *gInCol, float simCut, const float *MDK_W, bool complementary, bool variable) ");

    int i, x, pos, counter, num, lenBlock;
    newAlignment *newAlig = new newAlignment(*_alignment);


    // Reject columns with gaps number greater than the gap threshold.
    for (i = 0; i < _alignment->originalResidNumber; i++) {
        if (gInCol[i] > gapCut || MDK_W[i] < simCut)
            newAlig->saveResidues[i] = -1;
    }

    // Rescue residues based on their neighbouring residues. We are going to rescue those residues that would be rejected but have at least, 3 non-rejected residues.
    {
        // We're going to store a 5-value window of values that we are goint to move residue by residue.
        // This allows us to keep a reduced memory consumption, as there is no need to make a whole copy of the saveResidues array.
        std::deque<bool> rejectResiduesBuffer = std::deque<bool>(); // Here we store: True(1) if the residue was rejected, False(0) if it was accepted.
        std::deque<int> positionResidueBuffer = std::deque<int>(); // Here we store the position of the residues of the previous deque.

        // We're going to add the first 5 newAlig residues. That means, they are not rejected on the original _alignment.
        for (i = 0, num = 0; i < _alignment->originalResidNumber && num < 5; i++) {
            if (_alignment->saveResidues[i] == -1) continue;
            else {
                rejectResiduesBuffer.push_back(newAlig->saveResidues[i] == -1);
                positionResidueBuffer.push_back(i);
                num++;
            }
        }

        // Special case: Position 0 of newAlig
        if (num > 2) {
            if (rejectResiduesBuffer[0])
                newAlig->saveResidues[positionResidueBuffer[0]] =
                        // Compare the sum of the booleans to 0. If any of the neighbours is rejected, we don't have 3 non-rejected residues.
                        (rejectResiduesBuffer[1] +
                         rejectResiduesBuffer[2]) > 0 ? -1 : positionResidueBuffer[0];
        }
        // Special case: Position 1
        {
            // Special case: Position 1 of newAlig in case the alignment has more than 3 residues.
            if (num > 3) {
                if (rejectResiduesBuffer[1])
                    newAlig->saveResidues[positionResidueBuffer[1]] =
                            // Compare the sum of the booleans to 0. If any of the neighbours is rejected, we don't have 3 non-rejected residues.
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[2] +
                             rejectResiduesBuffer[3]) > 0 ? -1 : positionResidueBuffer[1];
            }
                // Special case: Position 1 of newAlig in case the alignment has 3 residues.
            else if (num > 2) {
                if (rejectResiduesBuffer[1])
                    newAlig->saveResidues[positionResidueBuffer[1]] =
                            // Compare the sum of the booleans to 0. If any of the neighbours is rejected, we don't have 3 non-rejected residues.
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[2]) > 0 ? -1 : positionResidueBuffer[1];
            }
        }
        // Special case: Position 2
        {
            // Special case: Position 2 of newAlig in case the alignment has more than 4 residues.
            if (num > 4) {
                if (rejectResiduesBuffer[2])
                    newAlig->saveResidues[positionResidueBuffer[2]] =
                            // Compare the sum of the booleans to 0. As we have 4 neighbours to compare and only need 3 of them to be non-rejected,
                            //      we can have 1 rejected residue on the neighbouring.
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[1] +
                             rejectResiduesBuffer[3] +
                             rejectResiduesBuffer[4]) > 1 ? -1 : positionResidueBuffer[2];
            }
                // Special case: Position 2 of newAlig in case the alignment has 4 residues
            else if (num > 3) {
                if (rejectResiduesBuffer[2])
                    newAlig->saveResidues[positionResidueBuffer[2]] =
                            // Compare the sum of the booleans to 0. As we have 4 neighbours to compare and only need 3 of them to be non-rejected,
                            //      we can have 1 rejected residue on the neighbouring.
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[1] +
                             rejectResiduesBuffer[3]) > 0 ? -1 : positionResidueBuffer[2];
            }
                // Special case: Position 2 of newAlig in case the alignment has 3 residues
            else if (num > 2) {
                if (rejectResiduesBuffer[2])
                    newAlig->saveResidues[positionResidueBuffer[2]] =
                            // Compare the sum of the booleans to 0. As we have 4 neighbours to compare and only need 3 of them to be non-rejected,
                            //      we can have 1 rejected residue on the neighbouring.
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[1]) > 0 ? -1 : positionResidueBuffer[2];
            }
        }

        // Move the window until it arrives to the end of the alignment.
        if (num == 5)
            for (; i < _alignment->originalResidNumber; i++) {
                if (_alignment->saveResidues[i] == -1) continue;
                    // If we find a new newAlig residue...
                else {
                    // We add one new value to each buffer and remove the oldest value from it, effectively moving the window.
                    rejectResiduesBuffer.pop_front();
                    rejectResiduesBuffer.push_back(newAlig->saveResidues[i] == -1);

                    positionResidueBuffer.pop_front();
                    positionResidueBuffer.push_back(i);

                    // If the new middle-point residue was going to be rejected...
                    if (rejectResiduesBuffer[2]) {
                        // Take a look at the neighbours of the new middle-point, positionResidueBuffer[2].
                        // As we stored the rejectResiduesBuffer as booleans, we can add them and compare the result.
                        // If the result is bigger than 1, means that we have rejected at least 2 neighbouring residues, thus, we cannot rescue this residue.
                        newAlig->saveResidues[positionResidueBuffer[2]] =
                                (rejectResiduesBuffer[0] +
                                 rejectResiduesBuffer[1] +
                                 rejectResiduesBuffer[3] +
                                 rejectResiduesBuffer[4]) > 1 ? -1 : positionResidueBuffer[2];
                    }
                }
            }

        // Special case: Position -1 of newAlig
        {
            if (num > 4) {
                if (rejectResiduesBuffer[3])
                    newAlig->saveResidues[positionResidueBuffer[3]] =
                            (rejectResiduesBuffer[1] +
                             rejectResiduesBuffer[2] +
                             rejectResiduesBuffer[4]) > 0 ? -1 : positionResidueBuffer[3];
            } else if (num > 3) {
                if (rejectResiduesBuffer[2])
                    newAlig->saveResidues[positionResidueBuffer[2]] =
                            (rejectResiduesBuffer[0] +
                             rejectResiduesBuffer[1] +
                             rejectResiduesBuffer[3]) > 0 ? -1 : positionResidueBuffer[2];
            }
        }
        // Special case: Position -2 of newAlig
        if (num > 4) {
            if (rejectResiduesBuffer[4])
                newAlig->saveResidues[positionResidueBuffer[4]] =
                        (rejectResiduesBuffer[2] +
                         rejectResiduesBuffer[3]) > 0 ? -1 : positionResidueBuffer[4];
        } else if (num > 3) {
            if (rejectResiduesBuffer[3])
                newAlig->saveResidues[positionResidueBuffer[4]] =
                        (rejectResiduesBuffer[1] +
                         rejectResiduesBuffer[2]) > 0 ? -1 : positionResidueBuffer[3];
        }
    }


    // Select blocks size based on user input. It can be set either to 5 or to a
    // variable number between 3 and 12 depending on alignment's length (1% alig)
    if (!variable)
        lenBlock = 5;
    else {
        lenBlock = utils::roundInt(_alignment->residNumber * 0.01F);
        lenBlock = lenBlock > 3 ? (lenBlock < 12 ? lenBlock : 12) : 3;
    }

    // Allow to change minimal block size
    blockSize = blockSize > 0 ? blockSize : lenBlock;

    // Keep only columns blocks bigger than an input columns block size

    for (i = 0, pos = 0, counter = 0; i < newAlig->originalResidNumber; i++) {

        if (_alignment->saveResidues[i] == -1) {
            pos++;
            continue;
        }
        if (newAlig->saveResidues[i] != -1) {
            pos++;
            counter++;
            continue;
        }

        if (counter < blockSize) {
            while (pos > 0) {
                newAlig->saveResidues[i - pos--] = -1;
            }
        }
        counter = 0;
        pos = 0;
    }

    if (counter < blockSize) {
        while (pos > 0) {
            newAlig->saveResidues[i - pos--] = -1;
        }
    }

    // If the flag -terminalony is set, apply a method to look for internal
    // boundaries and get back columns inbetween them, if they exist
    if (terminalGapOnly == true)
        if (!newAlig->Cleaning->removeOnlyTerminal())
            return NULL;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active
    if (complementary == true)
        newAlig->Cleaning->computeComplementaryAlig(true, false);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers 

    newAlig->Cleaning->removeAllGapsSeqsAndCols();

    return newAlig;
}

newAlignment *Cleaner::cleanOverlapSeq(float minimumOverlap, float *overlapSeq, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanOverlapSeq(float minimumOverlap, float *overlapSeq, bool complementary) ");

    newAlignment *newAlig = new newAlignment(*_alignment);
    int i;

    // Keep only those sequences with an overlap value equal or greater than
    // the minimum overlap value set by the user.
    for (i = 0; i < _alignment->originalSequenNumber; i++)
        if (overlapSeq[i] < minimumOverlap)
            newAlig->saveSequences[i] = -1;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active 
    if (complementary == true)
        newAlig->Cleaning->computeComplementaryAlig(false, true);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers
    newAlig->Cleaning->removeAllGapsSeqsAndCols();

    return newAlig;
}

newAlignment *Cleaner::cleanGaps(float baseLine, float gapsPct, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanGaps(float baseLine, float gapsPct, bool complementary) ");

    newAlignment *ret;
    double cut;

    // If gaps statistics are not calculated, we
    // calculate them 
    if (_alignment->Statistics->calculateGapStats() == false)
        return NULL;

    // Obtain the cut point using the given parameters
    cut = _alignment->Statistics->gaps->calcCutPoint(baseLine, gapsPct);

    // Once we have the cut value proposed, we call the
    // appropiate method to clean the newAlignment and, then,
    // generate the new newAlignment.
    ret = cleanByCutValueOverpass(cut, baseLine, _alignment->Statistics->gaps->getGapsWindow(), complementary);

    // Return a reference of the new newAlignment
    return ret;

}

newAlignment *Cleaner::cleanConservation(float baseLine, float conservationPct, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanConservation(float baseLine, float conservationPct, bool complementary) ");

    newAlignment *ret;
    float cut;

    // If conservation's statistics are not calculated,
    // we calculate them
    if (_alignment->Statistics->calculateConservationStats() != true)
        return NULL;
    // Calculate the cut point using the given parameters
    cut = (float) _alignment->Statistics->conservation->calcCutPoint(baseLine, conservationPct);

    // Once we have the cut value, we call the appropiate
    // method to clean the newAlignment and, then, generate
    // the new newAlignment
    ret = cleanByCutValueFallBehind(cut, baseLine, _alignment->Statistics->conservation->getMdkwVector(), complementary);
    // Return a reference of the new newAlignment
    return ret;

}

newAlignment *Cleaner::clean(float baseLine, float GapsPct, float conservationPct, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::clean(float baseLine, float GapsPct, float conservationPct, bool complementary) ");

    newAlignment *ret;
    float cutCons;
    double cutGaps;

    // If gaps statistics are not calculated, we calculate
    // them
    if (_alignment->Statistics->calculateGapStats() != true)
        return NULL;

    // If conservation's statistics are not calculated,
    // we calculate them
    if (_alignment->Statistics->calculateConservationStats() != true)
        return NULL;

    // Calculate the two cut points using the parameters
    cutGaps = _alignment->Statistics->gaps->calcCutPoint(baseLine, GapsPct);
    cutCons = _alignment->Statistics->conservation->calcCutPoint(baseLine, conservationPct);

    // Clean the alingment using the two cut values, the
    // gapsWindow and MDK_Windows vectors and the baseline
    // value
    ret = cleanByCutValueOverpassOrEquals(cutGaps,
                                          _alignment->Statistics->gaps->getGapsWindow(),
                                          baseLine,
                                          cutCons,
                                          _alignment->Statistics->conservation->getMdkwVector(),
                                          complementary);
    // Return a reference of the clean newAlignment object
    return ret;
}

newAlignment *Cleaner::cleanCompareFile(float cutpoint, float baseLine, float *vectValues, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanCompareFile(float cutpoint, float baseLine, float *vectValues, bool complementary) ");

    newAlignment *ret;
    float cut, *vectAux;

    // Allocate memory 
    vectAux = new float[_alignment->originalResidNumber];

    // Sort a copy of the vectValues vector, and take the
    // value at 100% - baseline position.
    utils::copyVect((float *) vectValues, vectAux, _alignment->originalResidNumber);
    utils::quicksort(vectAux, 0, _alignment->originalResidNumber - 1);
    cut = vectAux[(int) ((float) (_alignment->originalResidNumber - 1) * (100.0 - baseLine) / 100.0)];

    // We have to decide which is the smallest value
    // between the cutpoint value and the value from
    // the minimum percentage threshold */
    cut = utils::min(cutpoint, cut);

    // Clean the selected newAlignment using the input parameters.
    ret = cleanByCutValueFallBehind(cut, baseLine, vectValues, complementary);

    // Deallocate memory
    delete[] vectAux;

    // Return a refernce of the new newAlignment
    return ret;
}

bool Cleaner::calculateSpuriousVector(float overlap, float *spuriousVector) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("bool Cleaner::calculateSpuriousVector(float overlap, float *spuriousVector) ");

    int i, j, k, seqValue, ovrlap, hit;
    char indet;

    ovrlap = std::ceil(overlap * float(_alignment->originalSequenNumber - 1));

    if (spuriousVector == NULL)
        return false;
    // Depending on the kind of newAlignment, we have
    // different indetermination symbol
    if (_alignment->getAlignmentType() == SequenceTypes::AA)
        indet = 'X';
    else
        indet = 'N';
    // For each newAlignment's sequence, computes its overlap
    for (i = 0, seqValue = 0; i < _alignment->originalSequenNumber; i++, seqValue = 0) {
        // For each newAlignment's column, computes the overlap
        // between the selected sequence and the other ones
        for (j = 0; j < _alignment->originalResidNumber; j++) {

            // For sequences are before the sequence selected
            for (k = 0, hit = 0; k < i; k++) {
                // If the element of sequence selected is the same
                // that the element of sequence considered, computes
                // a hit
                if (_alignment->sequences[i][j] == _alignment->sequences[k][j])
                    hit++;

                    // If the element of sequence selected isn't a 'X' nor
                    // 'N' (indetermination) or a '-' (gap) and the element
                    // of sequence considered isn't a  a 'X' nor 'N'
                    // (indetermination) or a '-' (gap), computes a hit
                else if ((_alignment->sequences[i][j] != indet) && (_alignment->sequences[i][j] != '-')
                         && (_alignment->sequences[k][j] != indet) && (_alignment->sequences[k][j] != '-'))
                    hit++;
            }

            // For sequences are after the sequence selected
            for (k = (i + 1); k < _alignment->originalSequenNumber; k++) {
                // If the element of sequence selected is the same
                // that the element of sequence considered, computes
                // a hit 
                if (_alignment->sequences[i][j] == _alignment->sequences[k][j])
                    hit++;

                    // If the element of sequence selected isn't a 'X' nor
                    // 'N' (indetermination) or a '-' (gap) and the element
                    // of sequence considered isn't a  a 'X' nor 'N'
                    // (indetermination) or a '-' (gap), computes a hit
                else if ((_alignment->sequences[i][j] != indet) && (_alignment->sequences[i][j] != '-')
                         && (_alignment->sequences[k][j] != indet) && (_alignment->sequences[k][j] != '-'))
                    hit++;
            }
            // Finally, if the hit's number divided by number of
            // sequences minus one is greater or equal than
            // overlap's value, computes a column's hit.
            if (hit >= ovrlap)
                seqValue++;
        }

        // For each newAlignment's sequence, computes its spurious's
        // or overlap's value as the column's hits -for that
        // sequence- divided by column's number.
        spuriousVector[i] = ((float) seqValue / _alignment->residNumber);
    }

    // If there is not problem in the method, return true
    return true;
}


newAlignment *Cleaner::cleanSpuriousSeq(float overlapColumn, float minimumOverlap, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanSpuriousSeq(float overlapColumn, float minimumOverlap, bool complementary) ");

    float *overlapVector;
    newAlignment *newAlig;

    overlapVector = new float[_alignment->originalSequenNumber];

    // Compute the overlap's vector using the overlap column's value
    if (!calculateSpuriousVector(overlapColumn, overlapVector))
        return NULL;

    // Select and remove the sequences with a overlap less than threshold's overlap and create a new _alignmentemnt
    newAlig = cleanOverlapSeq(minimumOverlap, overlapVector, complementary);

    // Deallocate local memory
    delete[] overlapVector;

    return newAlig;
}

newAlignment *Cleaner::clean2ndSlope(bool complementarity) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::clean2ndSlope(bool complementarity) ");

    newAlignment *ret;

    int cut;

    //  If gaps statistics are not calculated, we calculate them 
    if (_alignment->Statistics->calculateGapStats() != true)
        return NULL;

    // We get the cut point using a automatic method for this purpose.
    cut = _alignment->Statistics->gaps->calcCutPoint2ndSlope();

    // Using the cut point calculates in last steps, weclean the newAlignment and generate a new Alignment
    ret = cleanByCutValueOverpass(cut, 0, _alignment->Statistics->gaps->getGapsWindow(), complementarity);

    // Returns the new newAlignment.
    return ret;
}

newAlignment *Cleaner::cleanCombMethods(bool complementarity, bool variable) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanCombMethods(bool complementarity, bool variable) ");

    float simCut, first20Point, last80Point, *simil, *vectAux;
    int i, j, acm, gapCut, *positions, *gaps;
    double inic, fin, vlr;

    // If gaps statistics are not calculated, we calculate them
    if (_alignment->Statistics->calculateGapStats() != true)
        return NULL;

    // Computes the gap cut point using a automatic method and at the same time, we get the gaps values from the newAlignment.
    gapCut = _alignment->Statistics->gaps->calcCutPoint2ndSlope();
    gaps = _alignment->Statistics->gaps->getGapsWindow();

    // If conservation's statistics are not calculated, we calculate them
    if (_alignment->Statistics->calculateConservationStats() != true)
        return NULL;

    /* Computes the conservations value for each column in the newAlignment. At the same time, the method get the vector with those values. */
//    _alignment->scons -> calculateVectors(/*_alignment->sequences,*/ _alignment->sgaps -> getGapsWindow());

    simil = _alignment->Statistics->conservation->getMdkwVector();

    // Allocate local memory and initializate it to -1
    positions = new int[_alignment->originalResidNumber];
    utils::initlVect(positions, _alignment->originalResidNumber, -1);
    // The method only selects columns with gaps number less or equal than the gap's cut point. Counts the number of columns that have been selected
    for (i = 0, acm = 0; i < _alignment->originalResidNumber; i++) {
        if (_alignment->saveResidues[i] == -1) continue;
        if (gaps[i] <= gapCut) {
            positions[i] = i;
            acm++;
        }
    }
    // Allocate local memory and save the similaritys values for the columns that have been selected
    vectAux = new float[acm];
    for (i = 0, j = 0; i < _alignment->originalResidNumber; i++)
        if (positions[i] != -1)
            vectAux[j++] = simil[i];

    // Sort the conservation's value vector.
    utils::quicksort(vectAux, 0, acm - 1);

    // ...and search for the vector points at the 20 and 80% of length.
    first20Point = 0;
    last80Point = 0;

    for (i = acm - 1, j = 1; i >= 0; i--, j++) {
        if ((((float) j / acm) * 100.0) <= 20.0)
            first20Point = vectAux[i];
        if ((((float) j / acm) * 100.0) <= 80.0)
            last80Point = vectAux[i];
    }

    // Computes the logaritmic's values for those points. Finally the method computes the similarity cut point using these values.
    inic = log10(first20Point);
    fin = log10(last80Point);
    vlr = ((inic - fin) / 10) + fin;
    simCut = (float) pow(10, vlr);

    // Clean the newAlignment and generate a new newAlignment object using the gaps cut and the similaritys cut values
    newAlignment *ret = cleanStrict(gapCut, _alignment->Statistics->gaps->getGapsWindow(),
                                    simCut, _alignment->Statistics->conservation->getMdkwVector(),
                                    complementarity, variable);

    // Deallocate local memory
    delete[] vectAux;
    delete[] positions;

    // Return a reference of the new newAlignment
    return ret;
}

newAlignment *Cleaner::cleanNoAllGaps(bool complementarity) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::cleanNoAllGaps(bool complementarity) ");

    newAlignment *ret;

    // If gaps statistics are not calculated, we calculate them
    if (_alignment->Statistics->calculateGapStats() != true)
        return NULL;

    // We want to conserve the columns with gaps' number less or equal than sequences' number - 1 
    ret = cleanByCutValueOverpass((_alignment->sequenNumber - 1), 0, _alignment->Statistics->gaps->getGapsWindow(), complementarity);

    // Returns the new newAlignment.
    return ret;

}

float Cleaner::getCutPointClusters(int clusterNumber) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("float Cleaner::getCutPointClusters(int clusterNumber) ");

    float max, min, avg, gMax, gMin, startingPoint, prevValue = 0, iter = 0;
    int i, j, clusterNum, *cluster, **seqs;

    // If the user wants only one cluster means that all
    // of sequences have to be in the same clauster.
    // Otherwise, if the users wants the maximum number of
    // clusters means that each sequence have to be in their
    // own cluster 
    if (clusterNumber == _alignment->sequenNumber) return 1;
    else if (clusterNumber == 1) return 0;

    // Ask for the sequence identities assesment
    if (_alignment->identities == NULL)
        calculateSeqIdentity();

    // Compute the maximum, the minimum and the average
    // identity values from the sequences
    for (i = 0, gMax = 0, gMin = 1, startingPoint = 0; i < _alignment->originalSequenNumber; i++) {
        if (_alignment->saveSequences[i] == -1) continue;

        for (j = 0, max = 0, avg = 0, min = 1; j < i; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            if (max < _alignment->identities[i][j])
                max = _alignment->identities[i][j];
            if (min > _alignment->identities[i][j])
                min = _alignment->identities[i][j];
            avg += _alignment->identities[i][j];
        }

        for (j = i + 1; j < _alignment->sequenNumber; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            if (max < _alignment->identities[i][j])
                max = _alignment->identities[i][j];
            if (min > _alignment->identities[i][j])
                min = _alignment->identities[i][j];
            avg += _alignment->identities[i][j];
        }

        startingPoint += avg / (_alignment->sequenNumber - 1);
        if (max > gMax) gMax = max;
        if (min < gMin) gMin = min;

    }
    // Take the starting point as the average value
    startingPoint /= _alignment->sequenNumber;

    // Compute and sort the sequence length
    seqs = new int *[_alignment->sequenNumber];
    for (i = 0; i < _alignment->sequenNumber; i++) {
        seqs[i] = new int[2];
        seqs[i][0] = utils::removeCharacter('-', _alignment->sequences[i]).size();
        seqs[i][1] = i;
    }
    utils::quicksort(seqs, 0, _alignment->sequenNumber - 1);

    // Create the data structure to store the different
    // clusters for a given thresholds
    cluster = new int[_alignment->sequenNumber];
    cluster[0] = seqs[_alignment->sequenNumber - 1][1];

    // Look for the optimal identity value to get the
    // number of cluster set by the user. To do that, the
    // method starts in the average identity value and moves
    // to the right or lefe side of this value depending on
    // if this value is so strict or not. We set an flag to
    // avoid enter in an infinite loop, if we get the same
    // value for more than 10 consecutive point without get
    // the given cluster number means that it's impossible
    // to get this cut-off and we need to stop the search
    do {
        clusterNum = 1;
        // Start the search
        for (i = _alignment->sequenNumber - 2; i >= 0; i--) {

            for (j = 0; j < clusterNum; j++)
                if (_alignment->identities[seqs[i][1]][cluster[j]] > startingPoint)
                    break;

            if (j == clusterNum) {
                cluster[j] = seqs[i][1];
                clusterNum++;
            }

        }
        // Given the cutoff point, if we get the same cluster
        // number or we have been getting for more than 10
        // consecutive times the same cutoff point, we stop
        if ((clusterNum == clusterNumber) || (iter > 10))
            break;
        else {
            // We have to move to the left side from the average
            // to decrease the cutoff value and get a smaller number
            // of clusters
            if (clusterNum > clusterNumber) {
                gMax = startingPoint;
                startingPoint = (gMax + gMin) / 2;
            } else {
                /* In the opposite side, we have to move to the right
                 * side from the cutting point to be more strict and get
                 * a bigger number of clusters */
                gMin = startingPoint;
                startingPoint = (gMax + gMin) / 2;
            }
            // If the clusters number from the previous iteration
            // is different from the current number, we put the
            // iteration number to 0 and store this new value
            if (prevValue != clusterNum) {
                iter = 0;
                prevValue = clusterNum;
                // Otherwise, we increase the iteration number to
                // avoid enter in an infinitve loop
            } else iter++;
        }
    } while (true);

    // Deallocate dinamic memory
    for (i = 0; i < _alignment->sequenNumber; i++) delete[] seqs[i];
    delete[] seqs;
    delete[] cluster;

    return startingPoint;
}

void Cleaner::setTrimTerminalGapsFlag(bool terminalOnly_) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::setTrimTerminalGapsFlag(bool terminalOnly_) ");
    terminalGapOnly = terminalOnly_;
}

void Cleaner::setBoundaries(int *boundaries_) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::setBoundaries(int *boundaries_) ");
    if (boundaries_ != NULL) {
        left_boundary = boundaries_[0];
        right_boundary = boundaries_[1];
    }
}

newAlignment *Cleaner::getClustering(float identityThreshold) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::getClustering(float identityThreshold) ");

    int i, j, *clustering;
    newAlignment *newAlig = new newAlignment(*_alignment);

    // Get the representative member for each cluster
    // given a maximum identity threshold
    clustering = calculateRepresentativeSeq(identityThreshold);

    // Put all sequences to be deleted and get back those
    // sequences that are representative for each cluster
    for (i = 0; i < _alignment->originalSequenNumber; i++) {
        if (_alignment->saveSequences[i] == -1) continue;
        newAlig->saveSequences[i] = -1;
    }
    for (i = 1; i <= clustering[0]; i++) {
        newAlig->saveSequences[clustering[i]] = clustering[i];
    }

    newAlig->sequenNumber = clustering[0];

    delete[] clustering;
    // Return the new alignment reference
    return newAlig;
}

newAlignment *Cleaner::removeColumns(int *columns, int init, int size, bool complementary) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("newAlignment *Cleaner::removeColumns(int *columns, int init, int size, bool complementary) ");

    string *matrixAux, *newSeqsName;
    newAlignment *newAlig = new newAlignment(*_alignment);
    int i, j;

    // Delete those range columns defines in the columns vector
    for (i = init; i < size + init; i += 2)
        for (j = columns[i]; j <= columns[i + 1]; j++)
            newAlig->saveResidues[j] = -1;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active
    if (complementary == true)
        newAlig->Cleaning->computeComplementaryAlig(true, false);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers
    newAlig->Cleaning->removeAllGapsSeqsAndCols();
    newAlig->updateSequencesAndResiduesNums();
    // Return the new alignment reference
    return newAlig;
}

newAlignment *Cleaner::removeSequences(int *seqs, int init, int size,
                                       bool complementary) {

    newAlignment *newAlig = new newAlignment(*_alignment);
    int i, j;

    // Delete those range of sequences defines by the seqs vector
    for (i = init; i < size + init; i += 2)
        for (j = seqs[i]; j <= seqs[i + 1]; j++)
            newAlig->saveSequences[j] = -1;

    // Once the columns/sequences selection is done, turn it around
    // if complementary flag is active
    if (complementary == true)
        newAlig->Cleaning->computeComplementaryAlig(false, true);

    // Check for any additional column/sequence to be removed
    // Compute new sequences and columns numbers
    newAlig->Cleaning->removeAllGapsSeqsAndCols();

    // Return the new alignment reference
    newAlig->updateSequencesAndResiduesNums();
    return newAlig;
}

bool Cleaner::removeOnlyTerminal(void) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("bool Cleaner::removeOnlyTerminal(void) ");

    int i;
    const int *gInCol;

    if ((left_boundary == -1) and (right_boundary == -1)) {
        // Get alignments gaps stats and copy it
        if (_alignment->Statistics->calculateGapStats() != true) {
            cerr << endl << "WARNING: Impossible to apply 'terminal-only' method"
                 << endl << endl;
            return false;
        }
        gInCol = _alignment->Statistics->gaps->getGapsWindow();

        // Identify left and right borders. First and last columns with no gaps
        for (i = 0; i < _alignment->originalResidNumber && gInCol[i] != 0; i++);
        left_boundary = i;

        for (i = _alignment->originalResidNumber - 1; i > -1 && gInCol[i] != 0; i--);
        right_boundary = i;
    } else if (left_boundary >= right_boundary) {
        debug.report(ErrorCode::LeftBoundaryBiggerThanRightBoundary, new std::string[2]{std::to_string(left_boundary), std::to_string(right_boundary)});
        return false;
    }

    // We increase the right boundary in one position because we use lower strict
    // condition to get back columns inbetween boundaries removed by any method
    right_boundary += 1;

    // Once the interal boundaries have been established, if these limits exist
    // then retrieved all columns inbetween both boundaries. Columns out of these
    // limits will remain selected or not depending on the algorithm applied
    for (i = left_boundary; i < right_boundary; i++)
        _alignment->saveResidues[i] = i;
    _alignment->updateSequencesAndResiduesNums();
    return true;
}

void Cleaner::removeSmallerBlocks(int blockSize) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::removeSmallerBlocks(int blockSize) ");
    // NOTE this method is not updated to use shared sequences. This shouldn't be used at all.
    int i, j, pos, block;

    if (blockSize == 0)
        return;

    // Traverse the newAlignment looking for blocks greater than BLOCKSIZE, everytime
    // than a column hasn't been selected, check whether the current block is big
    // enough to be kept or it should be removed from the final newAlignment
    for (i = 0, pos = 0, block = 0; i < _alignment->residNumber; i++) {
        if (_alignment->saveResidues[i] != -1)
            block++;
        else {
            // Remove columns from blocks smaller than input blocks size
            if (block < blockSize)
                for (j = pos; j <= i; j++)
                    _alignment->saveResidues[j] = -1;
            pos = i + 1;
            block = 0;
        }
    }

    // Check final block separately since it could happen than last block is not
    // big enough but because the loop end could provoke to ignore it
    if (block < blockSize)
        for (j = pos; j < i; j++)
            _alignment->saveResidues[j] = -1;
    return;
}

void Cleaner::removeAllGapsSeqsAndCols(bool seqs, bool cols) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::removeAllGapsSeqsAndCols(bool seqs, bool cols) ");
    int i, j, valid, gaps, counter;

    // Start checking the sequences.
    if (seqs) {
        for (i = 0, counter = 0; i < _alignment->originalSequenNumber; i++) {
            // Forget about sequences that are already rejected
            if (_alignment->saveSequences[i] == -1) continue;

            // Iterate over all residues
            for (j = 0; j < _alignment->sequences[i].length(); j++) {
                // Forget about residues that are already rejected
                if (_alignment->saveResidues[j] == -1)
                    continue;
                // Stop if one non-gap residue is found on the sequence
                if (_alignment->sequences[i][j] != '-')
                    break;
            }

            // If we haven't early-stopped due to finding a non-gap residue, j == sequenceLenght
            if (j == _alignment->sequences[i].length()) {
                if (keepSequences) {
                    debug.report(WarningCode::KeepingOnlyGapsColumn, new std::string[1]{_alignment->seqsName[i]});
                    counter++;
                } else {
                    debug.report(WarningCode::RemovingOnlyGapsColumn, new std::string[1]{_alignment->seqsName[i]});
                    _alignment->saveSequences[i] = -1;
                }
            } else counter++;
        }
        _alignment->sequenNumber = counter;
    }

    if (cols) {
        // Iterate over the residues
        for (j = 0, counter = 0; j < _alignment->originalResidNumber; j++) {
            // Forget about already discarded residues;
            if (_alignment->saveResidues[j] == -1) continue;

            // Check the residue position on each sequence.
            for (i = 0; i < _alignment->originalSequenNumber; i++) {
                // Forget about sequences that are already rejected
                if (_alignment->saveSequences[i] == -1)
                    continue;
                // Stop if a non gap residue is found on the column
                if (_alignment->sequences[i][j] != '-')
                    break;
            }

            // If we didn't early-stop due to finding a non-gap residue, j == sequenNumber
            if (i == _alignment->originalSequenNumber) {
                _alignment->saveResidues[j] = -1;
            } else counter++;
        }
        _alignment->residNumber = counter;
    }
}

void Cleaner::calculateSeqIdentity(void) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::calculateSeqIdentity(void) ");

    int i, j, k, hit, dst;
    char indet;

    // Depending on alignment type, indetermination symbol will be one or other
    indet = _alignment->getAlignmentType() & SequenceTypes::AA ? 'X' : 'N';

    // Create identities matrix to store identities scores
    _alignment->identities = new float *[_alignment->originalSequenNumber];

    // For each seq, compute its identity score against the others in the MSA
    for (i = 0; i < _alignment->originalSequenNumber; i++) {
        if (_alignment->saveSequences[i] == -1) continue;
        _alignment->identities[i] = new float[_alignment->originalSequenNumber];

        // It's a symmetric matrix, copy values that have been already computed
        for (j = 0; j < i; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            _alignment->identities[i][j] = _alignment->identities[j][i];
        }
        _alignment->identities[i][i] = 0;

        // Compute identity scores for the current sequence against the rest
        for (j = i + 1; j < _alignment->originalSequenNumber; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            for (k = 0, hit = 0, dst = 0; k < _alignment->residNumber; k++) {
                if (_alignment->saveResidues[k] == -1) continue;
                // If one of the two positions is a valid residue,
                // count it for the common length
                if (((_alignment->sequences[i][k] != indet) && (_alignment->sequences[i][k] != '-')) ||
                    ((_alignment->sequences[j][k] != indet) && (_alignment->sequences[j][k] != '-'))) {
                    dst++;
                    // If both positions are the same, count a hit
                    if (_alignment->sequences[i][k] == _alignment->sequences[j][k])
                        hit++;
                }
            }

            // Identity score between two sequences is the ratio of identical residues
            // by the total length (common and no-common residues) among them
            _alignment->identities[i][j] = (float) hit / dst;
        }
    }
}

void Cleaner::calculateRelaxedSeqIdentity(void) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::calculateRelaxedSeqIdentity(void) ");
    // Raw approximation of sequence identity computation designed for reducing
    // comparisons for huge alignemnts

    int i, j, k, hit;

    float inverseResidNumber = 1.F / _alignment->originalResidNumber;

    // Create identities matrix to store identities scores
    _alignment->identities = new float *[_alignment->originalSequenNumber];

    // For each seq, compute its identity score against the others in the MSA
    for (i = 0; i < _alignment->originalSequenNumber; i++) {
        if (_alignment->saveSequences[i] == -1) continue;
        _alignment->identities[i] = new float[_alignment->originalSequenNumber];

        // It's a symmetric matrix, copy values that have been already computed
        for (j = 0; j < i; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            _alignment->identities[i][j] = _alignment->identities[j][i];
        }
        _alignment->identities[i][i] = 0;

        // Compute identity score between the selected sequence and the others
        for (j = i + 1; j < _alignment->originalSequenNumber; j++) {
            if (_alignment->saveSequences[j] == -1) continue;
            for (k = 0, hit = 0; k < _alignment->originalResidNumber; k++) {
                if (_alignment->saveResidues[k] == -1) continue;
                // If both positions are the same, count a hit
                if (_alignment->sequences[i][k] == _alignment->sequences[j][k])
                    hit++;
            }
            // Raw identity score is computed as the ratio of identical residues between
            // alignment length 
            _alignment->identities[i][j] = hit * inverseResidNumber;
        }
    }
}

int *Cleaner::calculateRepresentativeSeq(float maximumIdent) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("int *Cleaner::calculateRepresentativeSeq(float maximumIdent) ");

    int i, j, pos, clusterNum, **seqs;
    int *cluster;
    static int *repres;
    float max;

    // Ask for the sequence identities assesment
    if (_alignment->identities == NULL)
        _alignment->Cleaning->calculateSeqIdentity();

    seqs = new int *[_alignment->originalSequenNumber];
    for (i = 0; i < _alignment->originalSequenNumber; i++) {
        if (_alignment->saveSequences[i] == -1) continue;
        seqs[i] = new int[2];
        seqs[i][0] = utils::removeCharacter('-', _alignment->sequences[i]).size();
        seqs[i][1] = i;
    }

    utils::quicksort(seqs, 0, _alignment->originalSequenNumber - 1);

    cluster = new int[_alignment->originalSequenNumber];
    cluster[0] = seqs[_alignment->originalSequenNumber - 1][1];
    clusterNum = 1;


    for (i = _alignment->originalSequenNumber - 2; i >= 0; i--) {
        if (_alignment->saveSequences[i] == -1) continue;

        for (j = 0, max = 0, pos = -1; j < clusterNum; j++) {
            if (_alignment->identities[seqs[i][1]][cluster[j]] > maximumIdent) {
                if (_alignment->identities[seqs[i][1]][cluster[j]] > max) {
                    max = _alignment->identities[seqs[i][1]][cluster[j]];
                    pos = j;
                }
            }
        }

        if (pos == -1) {
            cluster[j] = seqs[i][1];
            clusterNum++;
        }
    }

    repres = new int[clusterNum + 1];
    repres[0] = clusterNum;
    for (i = 0; i < clusterNum; i++)
        repres[i + 1] = cluster[i];

    // Deallocate dinamic memory
    for (i = 0; i < _alignment->originalSequenNumber; i++)
        delete[] seqs[i];

    delete[] cluster;
    delete[] seqs;


    return repres;

}

void Cleaner::computeComplementaryAlig(bool residues, bool sequences) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("void Cleaner::computeComplementaryAlig(bool residues, bool sequences) ");
    int i;

    if (residues)
        for (i = 0; i < _alignment->originalResidNumber; i++)
            _alignment->saveResidues[i] = (_alignment->saveResidues[i] == -1) ? i : -1;

    if (sequences)
        for (i = 0; i < _alignment->originalSequenNumber; i++)
            _alignment->saveSequences[i] = (_alignment->saveSequences[i] == -1) ? i : -1;
}


Cleaner::Cleaner(newAlignment *parent) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("Cleaner::Cleaner(newAlignment *parent) ");
    _alignment = parent;

    terminalGapOnly = false;
    keepSequences = false;

    blockSize = 0;

    left_boundary = -1;
    right_boundary = -1;
}

Cleaner::Cleaner(newAlignment *parent, Cleaner *mold) {
    // Create a timer that will report times upon its destruction
    //	which means the end of the current scope.
    StartTiming("Cleaner::Cleaner(newAlignment *parent, Cleaner *mold) ");
    _alignment = parent;

    terminalGapOnly = mold->terminalGapOnly;
    keepSequences = mold->keepSequences;
    blockSize = mold->blockSize;
    left_boundary = mold->left_boundary;
    right_boundary = mold->right_boundary;
}
