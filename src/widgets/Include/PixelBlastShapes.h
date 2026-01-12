#pragma once

enum StaticShapes : int
{
    // --- SQUARE ---
    /*
        # # . . .
        # # . . .
        . . . . .
    */
    Shape0 = 0x0303,

    // --- LINE I ---
    /*
        # # # # .
        . . . . .
        . . . . .
    */
    Shape1 = 0x0F,
    /*
        # . . . .
        # . . . .
        # . . . .
        # . . . .
    */
    Shape2 = 0x01010101,

    // --- ZETA (Z) ---
    /*
        . # # . .
        # # . . .
        . . . . .
    */
    Shape3 = 0x0603,
    /*
        # . . . .
        # # . . .
        . # . . .
    */
    Shape4 = 0x020301,

    // --- S-SHAPE ---
    /*
        # # . . .
        . # # . .
        . . . . .
    */
    Shape5 = 0x0306,
    /*
        . # . . .
        # # . . .
        # . . . .
    */
    Shape6 = 0x010302,

    // --- T-SHAPE ---
    /*
        . # . . .
        # # # . .
        . . . . .
    */
    Shape7 = 0x0207,
    /*
        # . . . .
        # # . . .
        # . . . .
    */
    Shape8 = 0x010301,
    /*
        # # # . .
        . # . . .
        . . . . .
    */
    Shape9 = 0x0702,
    /*
        . # . . .
        # # . . .
        . # . . .
    */
    Shape10 = 0x020302,

    // --- L-SHAPE ---
    /*
        # . . . .
        # # # . .
        . . . . .
    */
    Shape11 = 0x0107,
    /*
        # # . . .
        # . . . .
        # . . . .
    */
    Shape12 = 0x030101,
    /*
        # # # . .
        . . # . .
        . . . . .
    */
    Shape13 = 0x0704,
    /*
        . # . . .
        . # . . .
        # # . . .
    */
    Shape14 = 0x020203,

    // --- J-SHAPE ---
    /*
        . . # . .
        # # # . .
        . . . . .
    */
    Shape15 = 0x0407,
    /*
        # . . . .
        # . . . .
        # # . . .
    */
    Shape16 = 0x010103,
    /*
        # # # . .
        # . . . .
        . . . . .
    */
    Shape17 = 0x0701,
    /*
        # # . . .
        . # . . .
        . # . . .
    */
    Shape18 = 0x030202,
    /*
        # # # . .
        # # # . .
        # # # . .
    */
    Shape19 = 0x70707
};

constexpr int MaxShapes = 20;

constexpr inline int getShape(int idx)
{
    idx = qBound<int>(0, idx, MaxShapes - 1);
    switch(idx)
    {
        case 0:
            return StaticShapes::Shape0;
        case 1:
            return StaticShapes::Shape1;
        case 2:
            return StaticShapes::Shape2;
        case 3:
            return StaticShapes::Shape3;
        case 4:
            return StaticShapes::Shape4;
        case 5:
            return StaticShapes::Shape5;
        case 6:
            return StaticShapes::Shape6;
        case 7:
            return StaticShapes::Shape7;
        case 8:
            return StaticShapes::Shape8;
        case 9:
            return StaticShapes::Shape9;
        case 10:
            return StaticShapes::Shape10;
        case 11:
            return StaticShapes::Shape11;
        case 12:
            return StaticShapes::Shape12;
        case 13:
            return StaticShapes::Shape13;
        case 14:
            return StaticShapes::Shape14;
        case 15:
            return StaticShapes::Shape15;
        case 16:
            return StaticShapes::Shape16;
        case 17:
            return StaticShapes::Shape17;
        case 18:
            return StaticShapes::Shape18;
        case 19:
            return StaticShapes::Shape19;
        default:
            return StaticShapes::Shape0;
    }
}

inline int randomShapes()
{
    return getShape(QRandomGenerator::global()->bounded(0, MaxShapes));
}
