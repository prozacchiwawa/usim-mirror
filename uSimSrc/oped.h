/* uSim oped.h * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net *  * This program is free software; you can redistribute it and/or * modify it under the terms of the GNU General Public License * as published by the Free Software Foundation; either version 2 * of the License, or (at your option) any later version. *  * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the * GNU General Public License for more details. *  * You should have received a copy of the GNU General Public License * along with this program; if not, write to the Free Software * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. *//**********************************************************************/OPERATION(0x00, ED00, A, "", "", UOP2, NOP)OPERATION(0x01, ED01, A, "", "", UOP2, NOP)OPERATION(0x02, ED02, A, "", "", UOP2, NOP)OPERATION(0x03, ED03, A, "", "", UOP2, NOP)OPERATION(0x04, ED04, A, "", "", UOP2, NOP)OPERATION(0x05, ED05, A, "", "", UOP2, NOP)OPERATION(0x06, ED06, A, "", "", UOP2, NOP)OPERATION(0x07, ED07, A, "", "", UOP2, NOP)OPERATION(0x08, ED08, A, "", "", UOP2, NOP)OPERATION(0x09, ED09, A, "", "", UOP2, NOP)OPERATION(0x0A, ED0A, A, "", "", UOP2, NOP)OPERATION(0x0B, ED0B, A, "", "", UOP2, NOP)OPERATION(0x0C, ED0C, A, "", "", UOP2, NOP)OPERATION(0x0D, ED0D, A, "", "", UOP2, NOP)OPERATION(0x0E, ED0E, A, "", "", UOP2, NOP)OPERATION(0x0F, ED0F, A, "", "", UOP2, NOP)OPERATION(0x10, ED10, A, "", "", UOP2, NOP)OPERATION(0x11, ED11, A, "", "", UOP2, NOP)OPERATION(0x12, ED12, A, "", "", UOP2, NOP)OPERATION(0x13, ED13, A, "", "", UOP2, NOP)OPERATION(0x14, ED14, A, "", "", UOP2, NOP)OPERATION(0x15, ED15, A, "", "", UOP2, NOP)OPERATION(0x16, ED16, A, "", "", UOP2, NOP)OPERATION(0x17, ED17, A, "", "", UOP2, NOP)OPERATION(0x18, ED18, A, "", "", UOP2, NOP)OPERATION(0x19, ED19, A, "", "", UOP2, NOP)OPERATION(0x1A, ED1A, A, "", "", UOP2, NOP)OPERATION(0x1B, ED1B, A, "", "", UOP2, NOP)OPERATION(0x1C, ED1C, A, "", "", UOP2, NOP)OPERATION(0x1D, ED1D, A, "", "", UOP2, NOP)OPERATION(0x1E, ED1E, A, "", "", UOP2, NOP)OPERATION(0x1F, ED1F, A, "", "", UOP2, NOP)OPERATION(0x20, ED20, A, "", "", UOP2, NOP)OPERATION(0x21, ED21, A, "", "", UOP2, NOP)OPERATION(0x22, ED22, A, "", "", UOP2, NOP)OPERATION(0x23, ED23, A, "", "", UOP2, NOP)OPERATION(0x24, ED24, A, "", "", UOP2, NOP)OPERATION(0x25, ED25, A, "", "", UOP2, NOP)OPERATION(0x26, ED26, A, "", "", UOP2, NOP)OPERATION(0x27, ED27, A, "", "", UOP2, NOP)OPERATION(0x28, ED28, A, "", "", UOP2, NOP)OPERATION(0x29, ED29, A, "", "", UOP2, NOP)OPERATION(0x2A, ED2A, A, "", "", UOP2, NOP)OPERATION(0x2B, ED2B, A, "", "", UOP2, NOP)OPERATION(0x2C, ED2C, A, "", "", UOP2, NOP)OPERATION(0x2D, ED2D, A, "", "", UOP2, NOP)OPERATION(0x2E, ED2E, A, "", "", UOP2, NOP)OPERATION(0x2F, ED2F, A, "", "", UOP2, NOP)OPERATION(0x30, ED30, A, "", "", UOP2, NOP)OPERATION(0x31, ED31, A, "", "", UOP2, NOP)OPERATION(0x32, ED32, A, "", "", UOP2, NOP)OPERATION(0x33, ED33, A, "", "", UOP2, NOP)OPERATION(0x34, ED34, A, "", "", UOP2, NOP)OPERATION(0x35, ED35, A, "", "", UOP2, NOP)OPERATION(0x36, ED36, A, "", "", UOP2, NOP)OPERATION(0x37, ED37, A, "", "", UOP2, NOP)OPERATION(0x38, ED38, A, "", "", UOP2, NOP)OPERATION(0x39, ED39, A, "", "", UOP2, NOP)OPERATION(0x3A, ED3A, A, "", "", UOP2, NOP)OPERATION(0x3B, ED3B, A, "", "", UOP2, NOP)OPERATION(0x3C, ED3C, A, "", "", UOP2, NOP)OPERATION(0x3D, ED3D, A, "", "", UOP2, NOP)OPERATION(0x3E, ED3E, A, "", "", UOP2, NOP)OPERATION(0x3F, ED3F, A, "", "", UOP2, NOP)OPERATION(0x40, ED40, A, "", "IN B,(C)", UOP2, IN_B_iC)OPERATION(0x41, ED41, A, "", "OUT (C),B", UOP2, OUT_iC_B)OPERATION(0x42, ED42, A, "", "SBC HL,BC", UOP2, SBC_HL_BC)OPERATION(0x43, ED43NNNN, F, "", "LD (%4),BC", UOP2, LD_iNNNN_BC)OPERATION(0x44, ED44, A, "", "NEG", UOP2, NEG)OPERATION(0x45, ED45, A, "", "RETN", UOP2, RETN)OPERATION(0x46, ED46, A, "", "IM 0", UOP2, IM_0)OPERATION(0x47, ED47, A, "", "LD I,A", UOP2, LD_I_A)OPERATION(0x48, ED48, A, "", "IN C,(C)", UOP2, IN_C_iC)OPERATION(0x49, ED49, A, "", "OUT (C),C", UOP2, OUT_iC_C)OPERATION(0x4A, ED4A, A, "", "ADC HL,BC", UOP2, ADC_HL_BC)OPERATION(0x4B, ED4BNNNN, F, "", "LD BC,(%4)", UOP2, LD_BC_iNNNN)OPERATION(0x4C, ED4C, A, "", "", UOP2, UOP2)OPERATION(0x4D, ED4D, A, "", "RETI", UOP2, RETI)OPERATION(0x4E, ED4E, A, "", "", UOP2, UOP2)OPERATION(0x4F, ED4F, A, "", "", UOP2, UOP2)OPERATION(0x50, ED50, A, "", "IN D,(C)", UOP2, IN_D_iC)OPERATION(0x51, ED51, A, "", "OUT (C),D", UOP2, OUT_iC_D)OPERATION(0x52, ED52, A, "", "SBC HL,DE", UOP2, SBC_HL_DE)OPERATION(0x53, ED53NNNN, F, "", "LD (%4),DE", UOP2, LD_iNNNN_DE)OPERATION(0x54, ED54, A, "", "", UOP2, UOP2)OPERATION(0x55, ED55, A, "", "", UOP2, UOP2)OPERATION(0x56, ED56, A, "", "IM 1", UOP2, IM_1)OPERATION(0x57, ED57, A, "", "LD A,I", UOP2, LD_A_I)OPERATION(0x58, ED58, A, "", "IN E,(C)", UOP2, IN_E_iC)OPERATION(0x59, ED59, A, "", "OUT (C),E", UOP2, OUT_iC_E)OPERATION(0x5A, ED5A, A, "", "ADC HL,DE", UOP2, ADC_HL_DE)OPERATION(0x5B, ED5BNNNN, F, "", "LD DE,(%4)", UOP2, LD_DE_iNNNN)OPERATION(0x5C, ED5C, A, "", "", UOP2, UOP2)OPERATION(0x5D, ED5D, A, "", "", UOP2, UOP2)OPERATION(0x5E, ED5E, A, "", "IM 2", UOP2, IM_2)OPERATION(0x5F, ED5F, A, "", "", UOP2, UOP2)OPERATION(0x60, ED60, A, "", "IN H,(C)", UOP2, IN_H_iC)OPERATION(0x61, ED61, A, "", "OUT (C),H", UOP2, OUT_iC_H)OPERATION(0x62, ED62, A, "", "SBC HL,HL", UOP2, SBC_HL_HL)OPERATION(0x63, ED63, A, "", "", UOP2, UOP2)OPERATION(0x64, ED64, A, "", "", UOP2, UOP2)OPERATION(0x65, ED65, A, "", "", UOP2, UOP2)OPERATION(0x66, ED66, A, "", "", UOP2, UOP2)OPERATION(0x67, ED67, A, "", "RRD (HL)", UOP2, RRD_iHL)OPERATION(0x68, ED68, A, "", "IN L,(C)", UOP2, IN_L_iC)OPERATION(0x69, ED69, A, "", "OUT (C),L", UOP2, OUT_iC_L)OPERATION(0x6A, ED6A, A, "", "ADC HL,HL", UOP2, ADC_HL_HL)OPERATION(0x6B, ED6B, A, "", "", UOP2, UOP2)OPERATION(0x6C, ED6C, A, "", "", UOP2, UOP2)OPERATION(0x6D, ED6D, A, "", "", UOP2, UOP2)OPERATION(0x6E, ED6E, A, "", "", UOP2, UOP2)OPERATION(0x6F, ED6F, A, "", "RLD (HL)", UOP2, RLD_iHL)OPERATION(0x70, ED70, A, "", "", UOP2, UOP2)OPERATION(0x71, ED71, A, "", "", UOP2, UOP2)OPERATION(0x72, ED72, A, "", "SBC HL,SP", UOP2, SBC_HL_SP)OPERATION(0x73, ED73NNNN, F, "", "LD (%4),SP", UOP2, LD_iNNNN_SP)OPERATION(0x74, ED74, A, "", "", UOP2, UOP2)OPERATION(0x75, ED75, A, "", "", UOP2, UOP2)OPERATION(0x76, ED76, A, "", "", UOP2, UOP2)OPERATION(0x77, ED77, A, "", "", UOP2, UOP2)OPERATION(0x78, ED78, A, "", "IN A,(C)", UOP2, IN_A_iC)OPERATION(0x79, ED79, A, "", "OUT (C),A", UOP2, OUT_iC_A)OPERATION(0x7A, ED7A, A, "", "ADC HL,SP", UOP2, ADC_HL_SP)OPERATION(0x7B, ED7BNNNN, F, "", "LD SP,(%4)", UOP2, LD_SP_iNNNN)OPERATION(0x7C, ED7C, A, "", "", UOP2, UOP2)OPERATION(0x7D, ED7D, A, "", "", UOP2, UOP2)OPERATION(0x7E, ED7E, A, "", "", UOP2, UOP2)OPERATION(0x7F, ED7F, A, "", "", UOP2, UOP2)OPERATION(0x80, ED80, A, "", "", UOP2, UOP2)OPERATION(0x81, ED81, A, "", "", UOP2, UOP2)OPERATION(0x82, ED82, A, "", "", UOP2, UOP2)OPERATION(0x83, ED83, A, "", "", UOP2, UOP2)OPERATION(0x84, ED84, A, "", "", UOP2, UOP2)OPERATION(0x85, ED85, A, "", "", UOP2, UOP2)OPERATION(0x86, ED86, A, "", "", UOP2, UOP2)OPERATION(0x87, ED87, A, "", "", UOP2, UOP2)OPERATION(0x88, ED88, A, "", "", UOP2, UOP2)OPERATION(0x89, ED89, A, "", "", UOP2, UOP2)OPERATION(0x8A, ED8A, A, "", "", UOP2, UOP2)OPERATION(0x8B, ED8B, A, "", "", UOP2, UOP2)OPERATION(0x8C, ED8C, A, "", "", UOP2, UOP2)OPERATION(0x8D, ED8D, A, "", "", UOP2, UOP2)OPERATION(0x8E, ED8E, A, "", "", UOP2, UOP2)OPERATION(0x8F, ED8F, A, "", "", UOP2, UOP2)OPERATION(0x90, ED90, A, "", "", UOP2, UOP2)OPERATION(0x91, ED91, A, "", "", UOP2, UOP2)OPERATION(0x92, ED92, A, "", "", UOP2, UOP2)OPERATION(0x93, ED93, A, "", "", UOP2, UOP2)OPERATION(0x94, ED94, A, "", "", UOP2, UOP2)OPERATION(0x95, ED95, A, "", "", UOP2, UOP2)OPERATION(0x96, ED96, A, "", "", UOP2, UOP2)OPERATION(0x97, ED97, A, "", "", UOP2, UOP2)OPERATION(0x98, ED98, A, "", "", UOP2, UOP2)OPERATION(0x99, ED99, A, "", "", UOP2, UOP2)OPERATION(0x9A, ED9A, A, "", "", UOP2, UOP2)OPERATION(0x9B, ED9B, A, "", "", UOP2, UOP2)OPERATION(0x9C, ED9C, A, "", "", UOP2, UOP2)OPERATION(0x9D, ED9D, A, "", "", UOP2, UOP2)OPERATION(0x9E, ED9E, A, "", "", UOP2, UOP2)OPERATION(0x9F, ED9F, A, "", "", UOP2, UOP2)OPERATION(0xA0, EDA0, A, "", "LDI", UOP2, LDI)OPERATION(0xA1, EDA1, A, "", "CPI", UOP2, CPI)OPERATION(0xA2, EDA2, A, "", "INI", UOP2, INI)OPERATION(0xA3, EDA3, A, "", "OTI", UOP2, OTI)OPERATION(0xA4, EDA4, A, "", "", UOP2, UOP2)OPERATION(0xA5, EDA5, A, "", "", UOP2, UOP2)OPERATION(0xA6, EDA6, A, "", "", UOP2, UOP2)OPERATION(0xA7, EDA7, A, "", "", UOP2, UOP2)OPERATION(0xA8, EDA8, A, "", "LDD", UOP2, LDD)OPERATION(0xA9, EDA9, A, "", "CPD", UOP2, CPD)OPERATION(0xAA, EDAA, A, "", "IND", UOP2, IND)OPERATION(0xAB, EDAB, A, "", "OTD", UOP2, OTD)OPERATION(0xAC, EDAC, A, "", "", UOP2, UOP2)OPERATION(0xAD, EDAD, A, "", "", UOP2, UOP2)OPERATION(0xAE, EDAE, A, "", "", UOP2, UOP2)OPERATION(0xAF, EDAF, A, "", "", UOP2, UOP2)OPERATION(0xB0, EDB0, A, "", "LDIR", UOP2, LDIR)OPERATION(0xB1, EDB1, A, "", "CPIR", UOP2, CPIR)OPERATION(0xB2, EDB2, A, "", "INIR", UOP2, INIR)OPERATION(0xB3, EDB3, A, "", "OTIR", UOP2, OTIR)OPERATION(0xB4, EDB4, A, "", "", UOP2, UOP2)OPERATION(0xB5, EDB5, A, "", "", UOP2, UOP2)OPERATION(0xB6, EDB6, A, "", "", UOP2, UOP2)OPERATION(0xB7, EDB7, A, "", "", UOP2, UOP2)OPERATION(0xB8, EDB8, A, "", "LDDR", UOP2, LDDR)OPERATION(0xB9, EDB9, A, "", "CPDR", UOP2, CPDR)OPERATION(0xBA, EDBA, A, "", "INDR", UOP2, INDR)OPERATION(0xBB, EDBB, A, "", "OTDR", UOP2, OTDR)OPERATION(0xBC, EDBC, A, "", "", UOP2, UOP2)OPERATION(0xBD, EDBD, A, "", "", UOP2, UOP2)OPERATION(0xBE, EDBE, A, "", "", UOP2, UOP2)OPERATION(0xBF, EDBF, A, "", "", UOP2, UOP2)OPERATION(0xC0, EDC0, A, "", "", UOP2, UOP2)OPERATION(0xC1, EDC1, A, "", "", UOP2, UOP2)OPERATION(0xC2, EDC2, A, "", "", UOP2, UOP2)OPERATION(0xC3, EDC3, A, "", "", UOP2, UOP2)OPERATION(0xC4, EDC4, A, "", "", UOP2, UOP2)OPERATION(0xC5, EDC5, A, "", "", UOP2, UOP2)OPERATION(0xC6, EDC6, A, "", "", UOP2, UOP2)OPERATION(0xC7, EDC7, A, "", "", UOP2, UOP2)OPERATION(0xC8, EDC8, A, "", "", UOP2, UOP2)OPERATION(0xC9, EDC9, A, "", "", UOP2, UOP2)OPERATION(0xCA, EDCA, A, "", "", UOP2, UOP2)OPERATION(0xCB, EDCB, A, "", "", UOP2, UOP2)OPERATION(0xCC, EDCC, A, "", "", UOP2, UOP2)OPERATION(0xCD, EDCD, A, "", "", UOP2, UOP2)OPERATION(0xCE, EDCE, A, "", "", UOP2, UOP2)OPERATION(0xCF, EDCF, A, "", "", UOP2, UOP2)OPERATION(0xD0, EDD0, A, "", "", UOP2, UOP2)OPERATION(0xD1, EDD1, A, "", "", UOP2, UOP2)OPERATION(0xD2, EDD2, A, "", "", UOP2, UOP2)OPERATION(0xD3, EDD3, A, "", "", UOP2, UOP2)OPERATION(0xD4, EDD4, A, "", "", UOP2, UOP2)OPERATION(0xD5, EDD5, A, "", "", UOP2, UOP2)OPERATION(0xD6, EDD6, A, "", "", UOP2, UOP2)OPERATION(0xD7, EDD7, A, "", "", UOP2, UOP2)OPERATION(0xD8, EDD8, A, "", "", UOP2, UOP2)OPERATION(0xD9, EDD9, A, "", "", UOP2, UOP2)OPERATION(0xDA, EDDA, A, "", "", UOP2, UOP2)OPERATION(0xDB, EDDB, A, "", "", UOP2, UOP2)OPERATION(0xDC, EDDC, A, "", "", UOP2, UOP2)OPERATION(0xDD, EDDD, A, "", "", UOP2, UOP2)OPERATION(0xDE, EDDE, A, "", "", UOP2, UOP2)OPERATION(0xDF, EDDF, A, "", "", UOP2, UOP2)OPERATION(0xE0, EDE0, A, "", "", UOP2, UOP2)OPERATION(0xE1, EDE1, A, "", "", UOP2, UOP2)OPERATION(0xE2, EDE2, A, "", "", UOP2, UOP2)OPERATION(0xE3, EDE3, A, "", "", UOP2, UOP2)OPERATION(0xE4, EDE4, A, "", "", UOP2, UOP2)OPERATION(0xE5, EDE5, A, "", "", UOP2, UOP2)OPERATION(0xE6, EDE6, A, "", "", UOP2, UOP2)OPERATION(0xE7, EDE7, A, "", "", UOP2, UOP2)OPERATION(0xE8, EDE8, A, "", "", UOP2, UOP2)OPERATION(0xE9, EDE9, A, "", "", UOP2, UOP2)OPERATION(0xEA, EDEA, A, "", "", UOP2, UOP2)OPERATION(0xEB, EDEB, A, "", "", UOP2, UOP2)OPERATION(0xEC, EDEC, A, "", "", UOP2, UOP2)OPERATION(0xED, EDED, A, "", "", UOP2, UOP2)OPERATION(0xEE, EDEE, A, "", "", UOP2, UOP2)OPERATION(0xEF, EDEF, A, "", "", UOP2, UOP2)OPERATION(0xF0, EDF0, A, "", "", UOP2, UOP2)OPERATION(0xF1, EDF1, A, "", "", UOP2, UOP2)OPERATION(0xF2, EDF2, A, "", "", UOP2, UOP2)OPERATION(0xF3, EDF3, A, "", "", UOP2, UOP2)OPERATION(0xF4, EDF4, A, "", "", UOP2, UOP2)OPERATION(0xF5, EDF5, A, "", "", UOP2, UOP2)OPERATION(0xF6, EDF6, A, "", "", UOP2, UOP2)OPERATION(0xF7, EDF7, A, "", "", UOP2, UOP2)OPERATION(0xF8, EDF8, A, "", "", UOP2, UOP2)OPERATION(0xF9, EDF9, A, "", "", UOP2, UOP2)OPERATION(0xFA, EDFA, A, "", "", UOP2, UOP2)OPERATION(0xFB, EDFB, A, "", "", UOP2, UOP2)OPERATION(0xFC, EDFC, A, "", "", UOP2, UOP2)OPERATION(0xFD, EDFD, A, "", "", UOP2, UOP2)OPERATION(0xFE, EDFE, A, "", "", UOP2, UOP2)OPERATION(0xFF, EDFF, A, "", "", UOP2, UOP2)