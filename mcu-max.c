/*
 * mcu-max wrapper for micro-Max 4.8
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "mcu-max.h"

// Configuration

#define MCUMAX_HASHING_ENABLED
#define MCUMAX_HASH_TABLE_SIZE (1 << 24)

mcumax_callback Callback;
void *CallbackUserdata;

static int NodesLimit;

static int ValidMovesBufferSize;
static int ValidMovesBufferIndex;
static mcumax_square ValidMovesFrom;
static mcumax_square *ValidMovesBuffer;

/***************************************************************************/
/*                               micro-Max,                                */
/* A chess program smaller than 2KB (of non-blank source), by H.G. Muller  */
/***************************************************************************/
/* version 4.8 (1953 characters) features:                                 */
/* - recursive negamax search                                              */
/* - all-capture MVV/LVA quiescence search                                 */
/* - (internal) iterative deepening                                        */
/* - best-move-first 'sorting'                                             */
/* - a hash table storing score and best move                              */
/* - futility pruning                                                      */
/* - king safety through magnetic, frozen king in middle-game              */
/* - R=2 null-move pruning                                                 */
/* - keep hash and repetition-draw detection                               */
/* - better defense against passers through gradual promotion              */
/* - extend check evasions in inner nodes                                  */
/* - reduction of all non-Pawn, non-capture moves except hash move (LMR)   */
/* - full FIDE rules (expt under-promotion) and move-legality checking     */

#include <stdio.h>
#include <stdlib.h>

#define M 0x88
#define S 0x80
#define INF 8000

#define K(A, B) *(int *)(Zobrist + A + (B & 8) + S * (B & 7))
#define J(A) K(ToSqr + A, Board[ToSqr]) - K(FromSqr + A, Piece) - K(CaptSqr + A, Victim)

static int InputFrom;
static char InputTo;

/* board: half of 16x8+dummy */
char Board[129];
static int Nodes;
static int NonPawnMaterial;
static int Side = 0x10;

static int RootEval;
static int Rootep;

/* hash table, 16M+8 entries */
#ifdef MCUMAX_HASHING_ENABLED
static struct HashEntry
{
    int Key;
    int Score;
    char From;
    char To;
    char Draft;
} HashTab[MCUMAX_HASH_TABLE_SIZE];

static int HashKeyLo;
static int HashKeyHi;

/* hash translation table */
char Zobrist[1035];
#endif

/* relative piece values */
const char PieceVal[] = {0, 2, 2, 7, -1, 8, 12, 23};

/* 1st dir. in StepVecs[] per piece */
const char StepVecsIndices[] = {0, 7, -1, 11, 6, 8, 3, 6};

/* step-vector lists */
const char StepVecs[] = {-16, -15, -17, 0, 1, 16, 0, 1, 16, 15, 17, 0, 14, 18, 31, 33, 0};

/* initial piece setup */
const char PieceSetup[] = {6, 3, 5, 7, 4, 5, 3, 6};

/* recursive minimax search, Side=moving side, Depth=depth */
/* (Alpha,Beta)=window, Eval=current eval. score, epSqr=e.p. sqr. */
/* Eval=score, LastTo=prev.dest; HashKeyLo,HashKeyHi=hashkeys; return score */
int Search(int Alpha, int Beta, int Eval, int epSqr, int LastTo, int Depth)
{
    int StepVec;

    int BestScore;

    int Score;

    int IterDepth;

    int h;

    int SkipSqr;
    int RookSqr;

    int NewAlpha;

#ifdef MCUMAX_HASHING_ENABLED
    int LocalHashKeyLo = HashKeyLo;
    int LocalHashKeyHi = HashKeyHi;
#endif
    char Victim;
    char PieceType;
    char Piece;

    char FromSqr;
    char ToSqr;

    char BestFrom;
    char BestTo;

    char CaptSqr;
    char StartSqr;

    // +++ mcu-max changed
    if (Callback)
        Callback(CallbackUserdata);

        // --- mcu-max changed

        /* lookup pos. in hash table */
#ifdef MCUMAX_HASHING_ENABLED
    struct HashEntry *Hash = HashTab + ((HashKeyLo + Side * epSqr) & MCUMAX_HASH_TABLE_SIZE - 1);
#endif
    /* adj. window: delay bonus */
    Alpha--;

    /* change sides */
    Side ^= 0x18;

#ifdef MCUMAX_HASHING_ENABLED
    IterDepth = Hash->Draft;
    BestScore = Hash->Score;
    BestFrom = Hash->From;
    BestTo = Hash->To;                                                                /* resume at stored depth */
    if ((Hash->Key - HashKeyHi) | LastTo |                                            /* miss: other pos. or empty*/
        !((BestScore <= Alpha) | BestFrom & 8 && (BestScore >= Beta) | BestFrom & S)) /* or window incompatible */
        IterDepth = BestTo = 0;                                                       /* start iter. from scratch */
    BestFrom &= ~M;                                                                   /* start at best-move hint */
#else
    IterDepth = BestScore = BestFrom = BestTo = 0;
#endif

    /* iterative deepening loop */
    while ((IterDepth++ < Depth) || (IterDepth < 3) ||
           LastTo & (InputFrom == INF) &&
               ((Nodes < NodesLimit) & (IterDepth < 98) ||                       /* root: deepen upto time */
                (InputFrom = BestFrom, InputTo = (BestTo & ~M), IterDepth = 3))) /* time's up: go do best */
    {
        /* start scan at prev. best */
        FromSqr = StartSqr = BestFrom;

        /* request try noncastl. 1st */
        h = BestTo & S;

        /* Search null move */
        int P = (IterDepth < 3)
                    ? INF
                    : Search(-Beta, 1 - Beta, -Eval, S, 0, IterDepth - 3);

        /* Prune or stand-pat  */
        BestScore = (-P < Beta) | (NonPawnMaterial > 35) ? (IterDepth > 2) ? -INF : Eval : -P;

        /* node count (for timing) */
        Nodes++;

        do
        {
            /* scan board looking for */
            Piece = Board[FromSqr];

            /* own piece (inefficient!) */
            if (Piece & Side)
            {
                /* PieceType = piece type (set StepVec > 0) */
                StepVec = PieceType = (Piece & 7);

                /* first step vector f. piece */
                int j = StepVecsIndices[PieceType];

                /* loop over directions StepVecs[] */
                while (StepVec = ((PieceType > 2) & (StepVec < 0)) ? -StepVec : -StepVecs[++j])
                {
                    /* resume normal after best */
                replay:
                    /* (FromSqr,ToSqr)=move, (SkipSqr,RookSqr)=castl.R */
                    ToSqr = FromSqr;
                    SkipSqr = RookSqr = S;

                    do
                    {
                        /* ToSqr traverses ray, or: sneak in prev. best move */
                        CaptSqr = ToSqr = h ? (BestTo ^ h) : (ToSqr + StepVec);
                        if (ToSqr & M)
                            break;
                        /* board edge hit */
                        BestScore = (epSqr - S) & Board[epSqr] && ((ToSqr - epSqr) < 2) & ((epSqr - ToSqr) < 2) ? INF : BestScore; /* bad castling             */
                                                                                                                                   /* shift capt.sqr. CaptSqr if e.p.*/
                        if ((PieceType < 3) & (ToSqr == epSqr))
                            CaptSqr ^= 0x10;

                        Victim = Board[CaptSqr];

                        /* capt. own, bad pawn mode */
                        if (Victim & Side | (PieceType < 3) & !(ToSqr - FromSqr & 7) - !Victim)
                            break;

                        /* value of capt. piece Victim */
                        int i = 37 * PieceVal[Victim & 7] + (Victim & 0xc0);

                        /* K capture */
                        BestScore = (i < 0) ? INF : BestScore;

                        /* abort on fail high */
                        if ((BestScore >= Beta) & (IterDepth > 1))
                            goto cutoff;

                        /* MVV/LVA scoring */
                        Score = (IterDepth - 1) ? Eval : i - PieceType;
                        /* remaining depth */
                        if ((IterDepth - !Victim) > 1)
                        {
                            Score = (PieceType < 6) ? Board[FromSqr + 8] - Board[ToSqr + 8] : 0; /* center positional pts. */
                            Board[RookSqr] = Board[CaptSqr] = Board[FromSqr] = 0;

                            /* do move, set non-virgin */
                            Board[ToSqr] = Piece | 0x20;
                            if (!(RookSqr & M))
                            {
                                /* castling: put R & score */
                                Board[SkipSqr] = Side + 6;
                                Score += 50;
                            }

                            /* penalize mid-game K move */
                            Score -= (PieceType - 4) | (NonPawnMaterial > 29) ? 0 : 20;

                            /* pawns: */
                            if (PieceType < 3)
                            {
                                Score -= 9 * (((FromSqr - 2) & M || Board[FromSqr - 2] - Piece) +   /* structure, undefended    */
                                              ((FromSqr + 2) & M || Board[FromSqr + 2] - Piece) - 1 /* squares plus bias */
                                              + (Board[FromSqr ^ 16] == Side + 36))                 /* kling to non-virgin King */
                                         - (NonPawnMaterial >> 2);                                  /* end-game Pawn-push bonus */

                                /* promotion or 6/7th bonus */
                                NewAlpha = ToSqr + StepVec + 1 & S
                                               ? (647 - PieceType)
                                               : 2 * (Piece & (ToSqr + 0x10) & 0x20);
                                Board[ToSqr] += NewAlpha;

                                /* change piece, add score */
                                i += NewAlpha;
                            }
                            Score += Eval + i;

                            /* new eval and alpha */
                            NewAlpha = BestScore > Alpha ? BestScore : Alpha;

                            /* update hash key */
#ifdef MCUMAX_HASHING_ENABLED
                            HashKeyLo += J(0);
                            HashKeyHi += J(8) + RookSqr - S;
#endif

                            int C = IterDepth - 1 - ((IterDepth > 5) & (PieceType > 2) & !Victim & !h);
                            /* extend 1 ply if in check */
                            C = (NonPawnMaterial > 29) | (IterDepth < 3) | (P - INF) ? C : IterDepth;

                            int s;
                            do
                                /* recursive eval. of reply */
                                s = (C > 2) | (Score > NewAlpha)
                                        ? -Search(-Beta, -NewAlpha, -Score, SkipSqr, 0, C)
                                        : Score; /* or fail low if futile */
                            while ((s > Alpha) & (++C < IterDepth));

                            Score = s;
                            /* move pending & in root: */
                            // +++ mcu-max changed
                            // if (LastTo &&
                            //     (InputFrom - INF) &&
                            //     (Score + INF) &&
                            //     (FromSqr == InputFrom) & (ToSqr == InputTo))
                            if (LastTo &&
                                (InputFrom - INF) &&
                                (Score + INF))
                            {
                                if (ValidMovesBuffer &&
                                    (ValidMovesBufferIndex < (ValidMovesBufferSize - 5)))
                                {
                                    if (FromSqr != ValidMovesFrom)
                                    {
                                        if (ValidMovesFrom != MCUMAX_INVALID)
                                            ValidMovesBuffer[ValidMovesBufferIndex++] = MCUMAX_INVALID;

                                        ValidMovesBuffer[ValidMovesBufferIndex++] = FromSqr;
                                        ValidMovesFrom = FromSqr;
                                    }

                                    ValidMovesBuffer[ValidMovesBufferIndex++] = ToSqr;
                                }

                                if ((FromSqr == InputFrom) & (ToSqr == InputTo))
                                // --- mcu-max changed
                                {
                                    RootEval = -Eval - i;

                                    /* exit if legal & found */
                                    Rootep = SkipSqr;

                                    /* lock game in hash as draw */
#ifdef MCUMAX_HASHING_ENABLED
                                    Hash->Draft = 99;
                                    Hash->Score = 0;
#endif
                                    NonPawnMaterial += i >> 7;

                                    /* captured non-P material */
                                    return Beta;
                                }
                                // +++ mcu-max changed
                            }
                            // --- mcu-max changed

                            /* restore hash key */
#ifdef MCUMAX_HASHING_ENABLED
                            HashKeyLo = LocalHashKeyLo;
                            HashKeyHi = LocalHashKeyHi;
#endif

                            /* undo move,RookSqr can be dummy */
                            Board[RookSqr] = Side + 6;
                            Board[SkipSqr] = Board[ToSqr] = 0;
                            Board[FromSqr] = Piece;
                            Board[CaptSqr] = Victim;
                        }

                        if (Score > BestScore)
                        {
                            /* new best, update max,best */
                            BestScore = Score;
                            BestFrom = FromSqr;
                            /* mark double move with S */
                            BestTo = ToSqr | S & SkipSqr;
                        }

                        if (h)
                        {
                            /* redo after doing old best */
                            h = 0;
                            goto replay;
                        }

                        if ((FromSqr + StepVec - ToSqr) | (Piece & 0x20) |                                    /* not 1st step, moved before */
                            (PieceType > 2) & ((PieceType - 4) | (j - 7) ||                                   /* no P & no lateral K move, */
                                               Board[RookSqr = (FromSqr + 3) ^ (StepVec >> 1) & 7] - Side - 6 /* no virgin R in corner RookSqr, */
                                               || Board[RookSqr ^ 1] | Board[RookSqr ^ 2])                    /* no 2 empty sq. next to R */
                        )
                            /* fake capt. for nonsliding */
                            Victim += PieceType < 5;
                        else
                            /* enable e.p. */
                            SkipSqr = ToSqr;

                        /* if not capt. continue ray */
                    } while (!Victim);
                }
            }
            /* next sqr. of board, wrap */
        } while ((FromSqr = (FromSqr + 9) & ~M) - StartSqr);

    cutoff:
        /* mate holds to any depth */
        if ((BestScore > (INF - M)) | (BestScore < (M - INF)))
            IterDepth = 98;

        /* best loses K: (stale)mate */
        BestScore = (BestScore + INF) | (P == INF) ? BestScore : 0;

        /* protect game history */
#ifdef MCUMAX_HASHING_ENABLED
        if (Hash->Draft < 99)
        {
            /* always store in hash tab */
            Hash->Key = HashKeyHi;
            Hash->Score = BestScore;
            Hash->Draft = IterDepth;

            /* move, type (bound/exact),*/
            Hash->From = BestFrom | 8 * (BestScore > Alpha) | S * (BestScore < Beta);
            Hash->To = BestTo;

            /*if(LastTo)printf("%2d ply, %9d searched, score=%6d by %c%c%c%c\depth",IterDepth-1,Nodes-S,BestScore,
                'Hash'+(BestFrom&7),'8'-(BestFrom>>4),'Hash'+(BestTo&7),'8'-(BestTo>>4&7)); /* uncomment for Kibitz */
        }
#endif

        /* encoded in BestFrom S,8 bits */
    }

    /* change sides back */
    Side ^= 0x18;

    /* delayed-loss bonus */
    return BestScore += (BestScore < Eval);
}

// mcu-max wrapper

#define MCUMAX_BOARD_MASK 0x88
#define MCUMAX_INVALID 0x80

#define MCUMAX_MOVED 0x20

#define MCUMAX_SIDE_BLACK 0x08
#define MCUMAX_SIDE_WHITE 0x10

#define MCUMAX_INFINITY 8000

void mcumax_reset()
{
    for (int x = 0; x < 8; x++)
    {
        // Setup pieces (left side)
        Board[0x10 * 7 + x] = MCUMAX_WHITE | PieceSetup[x];
        Board[0x10 * 6 + x] = MCUMAX_WHITE | MCUMAX_PAWN_UPSTREAM;
        for (int y = 2; y < 6; y++)
            Board[0x10 * y + x] = MCUMAX_EMPTY;
        Board[0x10 * 1 + x] = MCUMAX_BLACK | MCUMAX_PAWN_DOWNSTREAM;
        Board[0x10 * 0 + x] = MCUMAX_BLACK | PieceSetup[x];

        // Setup weights (right side)
        for (int y = 0; y < 8; y++)
            Board[16 * y + x + 8] = (x - 4) * (x - 4) + (int)((y - 3.5) * (y - 3.5));
    }

    RootEval = 0;
    Rootep = MCUMAX_INVALID;
    NonPawnMaterial = 0;
    Side = MCUMAX_SIDE_WHITE;

    memset(HashTab, 0, sizeof(HashTab));

#ifdef MCUMAX_HASHING_ENABLED
    srand(1);
    for (int i = sizeof(Zobrist); i > MCUMAX_BOARD_MASK; i--)
        Zobrist[i] = rand() >> 9;
#endif
}

mcumax_square mcumax_set_piece(mcumax_square square, mcumax_piece piece)
{
    if (square & MCUMAX_BOARD_MASK)
        return square;

    Board[square] = piece ? piece | MCUMAX_MOVED : piece;

    return square + 1;
}

mcumax_piece mcumax_get_piece(mcumax_square square)
{
    if (square & MCUMAX_BOARD_MASK)
        return MCUMAX_EMPTY;

    return Board[square] & 0x1f;
}

void mcumax_set_fen_position(const char *fen_string)
{
    mcumax_reset();

    int field_index = 0;
    int board_index = 0;

    char c;
    while (c = *fen_string++)
    {
        if (c == ' ')
        {
            if (field_index < 4)
                field_index++;

            continue;
        }

        switch (field_index)
        {
        case 0:
            switch (c)
            {
            case '8':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '7':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '6':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '5':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '4':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '3':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '2':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
            case '1':
                board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);
                break;

            case 'P':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_PAWN_UPSTREAM);
                break;

            case 'N':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_KNIGHT);
                break;

            case 'B':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_BISHOP);
                break;

            case 'R':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_ROOK);
                break;

            case 'Q':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_QUEEN);
                break;

            case 'K':
                board_index = mcumax_set_piece(board_index, MCUMAX_WHITE | MCUMAX_KING);
                break;

            case 'p':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_PAWN_DOWNSTREAM);
                break;

            case 'n':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_KNIGHT);
                break;

            case 'b':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_BISHOP);
                break;

            case 'r':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_ROOK);
                break;

            case 'q':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_QUEEN);
                break;

            case 'k':
                board_index = mcumax_set_piece(board_index, MCUMAX_BLACK | MCUMAX_KING);
                break;

            case '/':
                if (board_index < 0x80)
                    board_index = (board_index & 0xf0) + 0x10;
                break;
            }
            break;

        case 1:
            switch (c)
            {
            case 'w':
                Side = MCUMAX_SIDE_WHITE;
                break;

            case 'b':
                Side = MCUMAX_SIDE_BLACK;
                break;
            }
            break;

        case 2:
            switch (c)
            {
            case 'K':
                Board[0x74] &= ~MCUMAX_MOVED;
                Board[0x77] &= ~MCUMAX_MOVED;
                break;

            case 'Q':
                Board[0x74] &= ~MCUMAX_MOVED;
                Board[0x70] &= ~MCUMAX_MOVED;
                break;

            case 'k':
                Board[0x04] &= ~MCUMAX_MOVED;
                Board[0x07] &= ~MCUMAX_MOVED;
                break;

            case 'q':
                Board[0x04] &= ~MCUMAX_MOVED;
                Board[0x00] &= ~MCUMAX_MOVED;
                break;
            }
            break;

        case 3:
            switch (c)
            {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
                Rootep &= 0x7f;
                Rootep |= (c - 'a');
                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
                Rootep &= 0x7f;
                Rootep |= 16 * ('8' - c);
                break;
            }
            break;
        }
    }
}

mcumax_piece mcumax_get_current_side()
{
    return Side == MCUMAX_SIDE_BLACK ? MCUMAX_WHITE : MCUMAX_BLACK;
}

void mcumax_set_callback(mcumax_callback callback, void *userdata)
{
    Callback = callback;
    CallbackUserdata = userdata;
}

void mcumax_get_valid_moves(mcumax_square *valid_moves_buffer, int valid_moves_buffer_size)
{
    ValidMovesBufferSize = valid_moves_buffer_size;
    ValidMovesBufferIndex = 0;
    ValidMovesBuffer = valid_moves_buffer;
    ValidMovesFrom = MCUMAX_INVALID;

    InputFrom = MCUMAX_INVALID;
    InputTo = MCUMAX_INVALID;

    Search(-MCUMAX_INFINITY, MCUMAX_INFINITY, RootEval, Rootep, 1, 3);

    ValidMovesBuffer[ValidMovesBufferIndex++] = MCUMAX_INVALID;
    ValidMovesBuffer[ValidMovesBufferIndex++] = MCUMAX_INVALID;
}

bool mcumax_play_move(mcumax_square move_from, mcumax_square move_to)
{
    ValidMovesBuffer = NULL;

    InputFrom = move_from;
    InputTo = move_to;
    Nodes = MCUMAX_BOARD_MASK;

    Search(-MCUMAX_INFINITY, MCUMAX_INFINITY, RootEval, Rootep, 1, 3);

    return (RootEval != INF);
}

bool mcumax_play_best_move(int nodes_limit,
                           mcumax_square *move_from, mcumax_square *move_to)
{
    NodesLimit = nodes_limit;
    ValidMovesBuffer = NULL;

    InputFrom = MCUMAX_INFINITY;
    Nodes = MCUMAX_BOARD_MASK;

    int Eval = Search(-MCUMAX_INFINITY, MCUMAX_INFINITY, RootEval, Rootep, 1, 3);

    *move_from = InputFrom;
    *move_to = InputTo;

    return (RootEval != INF);
}

void mcumax_stop_search()
{
    NodesLimit = 0;
}
