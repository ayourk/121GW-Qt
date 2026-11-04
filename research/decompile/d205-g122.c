// ==== HY_Poll @ 08006124  [GPIOF]

/* main loop: PF3 (DO = IRQ while CS high) high -> HY_Service; capacitance extra pass. high */

undefined4 HY_Poll(void)

{
  ushort uVar1;
  undefined4 unaff_r7;
  
  uVar1 = GPIOF.IDR._0_2_;
  if ((int)((uint)uVar1 << 0x1c) < 0) {
    HY_Service();
  }
  if ((g_mode == '\f') && (g_range < 5)) {
    Mode_Cap_Poll();
  }
  return unaff_r7;
}


// ==== HY_Service @ 0800614e

/* read 52 registers 0x00-0x33, INTF = 0, flags = INTF & INTE -> AD1 / RMS / peak / counter
   handlers. high */

undefined4 HY_Service(void)

{
  undefined4 unaff_r7;
  
  HY_Read(&g_hy_mirror,0,0x34);
  DAT_20007dc4 = 0;
  HY_Write(&DAT_20007dc4,0x1e,1);
  DAT_2000807f = DAT_200077ee & DAT_200077ef;
  DAT_2000807d = DAT_200077f0;
  DAT_2000807e = DAT_200077f9;
  if ((int)((uint)DAT_2000807f << 0x1d) < 0) {
    HY_OnAD1();
  }
  if ((int)((uint)DAT_2000807f << 0x1b) < 0) {
    HY_OnRMS();
  }
  if (((int)((uint)DAT_2000807e << 0x1d) < 0) && (DAT_200080ba == '\0')) {
    HY_OnPeak();
  }
  if ((int)((uint)DAT_2000807f << 0x1f) < 0) {
    HY_OnCounter();
  }
  return unaff_r7;
}


// ==== HY_OnAD1 @ 080061e4

/* AD1 conversion ready (AD1F). high (dispatch) */

undefined4 HY_OnAD1(void)

{
  undefined4 unaff_r7;
  
  DAT_20007d74 = HY_AD1Signed();
  DAT_20007dcc = DAT_20007d74;
  DAT_20008081 = HY_AD1Average(DAT_20007d74);
  if (((g_mode == '\x16') || (g_mode == '\x17')) || (g_mode == '\x18')) {
    Mode_DCVA_OnAD1();
    return unaff_r7;
  }
  if (((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) {
    Mode_ACVA_OnAD1();
    return unaff_r7;
  }
  if (g_mode == '\0') {
    Mode_LowZ_OnAD1();
    return unaff_r7;
  }
  if (g_mode != '\x01') {
    if (g_mode != '\x02') {
      if (g_mode == '\x03') {
        Mode_DCmV_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\x04') {
        Mode_ACmV_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\x05') {
        Mode_Temp_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\t') {
        Mode_Ohm_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\n') {
        Mode_Continuity_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\v') {
        Mode_Diode_OnAD1();
        return unaff_r7;
      }
      if (g_mode == '\x10') {
LAB_080062f2:
        Mode_ACI_OnAD1();
        return unaff_r7;
      }
      if (g_mode != '\x11') {
        if (g_mode == '\x12') goto LAB_080062f2;
        if (g_mode != '\x13') {
          if (g_mode == '\x14') goto LAB_080062f2;
          if (g_mode != '\x15') {
            return unaff_r7;
          }
        }
      }
      Mode_DCI_OnAD1();
      return unaff_r7;
    }
    if ((DAT_200080ba == '\0') || ((DAT_200080ba != '\0' && (DAT_200080bd == '\0')))) {
      Mode_ACV_OnAD1();
      return unaff_r7;
    }
  }
  Mode_DCV_OnAD1();
  return unaff_r7;
}


// ==== HY_AD1Signed @ 080062f8

/* 24-bit AD1 from the mirror; sign conversion -(0xFFFFFF - raw) for raw > 0x800000 (off by one: -1
   reads 0; 0x800000 stays positive). high */

uint HY_AD1Signed(void)

{
  uint uVar1;
  
  uVar1 = (uint)g_hy_mirror + (uint)DAT_200077d1 * 0x100 + (uint)DAT_200077d2 * 0x10000;
  DAT_20007dc8 = uVar1;
  if (0x800000 < uVar1) {
    uVar1 = -(0xffffff - uVar1);
  }
  return uVar1;
}


// ==== HY_AD1Average @ 08006342

/* 16 samples (DCV, DCmV, DC current), Ohm 8 (range 5: 1); returns true when an average is ready
   (0x20007dd4). high */

bool HY_AD1Average(int param_1)

{
  bool bVar1;
  byte bVar2;
  
  bVar2 = 0x10;
  if ((((g_mode == '\x11') || (g_mode == '\x13')) || (g_mode == '\x15')) ||
     ((g_mode == '\x01' || (g_mode == '\x03')))) {
    bVar2 = 0x10;
  }
  else if (g_mode == '\t') {
    if (g_range == '\x05') {
      bVar2 = 1;
    }
    else if (g_range == '\x06') {
      bVar2 = 8;
    }
    else {
      bVar2 = 8;
    }
  }
  if (DAT_20008080 == '\0') {
    DAT_20007dd0 = param_1;
  }
  else {
    DAT_20007dd0 = param_1 + DAT_20007dd0;
  }
  DAT_20008080 = DAT_20008080 + 1;
  bVar1 = bVar2 <= DAT_20008080;
  if (bVar1) {
    g_ad1_average = DAT_20007dd0 / (int)(uint)DAT_20008080;
    DAT_20008080 = 0;
    DAT_20007dd0 = 0;
  }
  return bVar1;
}


// ==== Cal_Apply @ 08006418

/* average -> offset (Cal_Offset) -> gain (Cal_Gain) -> 0x20007d64. high */

undefined4 Cal_Apply(void)

{
  undefined4 unaff_r7;
  
  if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
      (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) && (DAT_2000808c != '\0'))
  {
    g_reading = g_ad1_average;
  }
  else {
    g_reading = Cal_Offset(g_ad1_average);
    if ((((g_mode != '\r') && (g_mode != '\x0e')) &&
        ((g_mode != '\x0f' && (((g_mode != '\x16' && (g_mode != '\x17')) && (g_mode != '\x18'))))))
       || (DAT_2000808e == '\0')) {
      g_reading = Cal_Gain(g_reading);
    }
  }
  return unaff_r7;
}


// ==== Reading_FastPath @ 080064d6

/* every sample: offset + gain on the raw value -> 0x20007d68 (bargraph). high */

undefined4 Reading_FastPath(void)

{
  undefined4 unaff_r7;
  
  if ((((g_mode != '\x16') && (g_mode != '\x17')) && (g_mode != '\x18')) &&
     (((g_mode != '\r' && (g_mode != '\x0e')) && (g_mode != '\x0f')))) {
    g_reading_fast = Cal_SubtractOffset(DAT_20007d74);
    g_reading_fast = Cal_DivideGain(g_reading_fast);
    DAT_2000805e = 1;
  }
  return unaff_r7;
}


// ==== Mode_LowZ_OnAD1 @ 08006540

/* medium */

undefined4 Mode_LowZ_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008098 == '\0') {
    Mode_DCV_OnAD1();
  }
  else {
    Mode_ACV_OnAD1();
  }
  return unaff_r7;
}


// ==== LowZ_AutoAcDc @ 08006558

/* LowZ auto AC/DC search (main, after each Display_Update in mode 0): |reading| <= 100 counts (30
   once locked, 0x20008099) and not OL -> reading 0, unlock, and every 5 quiet displays (0x20000239)
   toggle DC/AC 0x20008098 + Mode_ApplyChange(0); larger or OL -> reset that counter (0800662c) and,
   if not locked, show 0 for 5 displays (0x2000023a) then lock. So the display reads 0 for 5 updates
   after a signal appears. medium */

void LowZ_AutoAcDc(void)

{
  int iVar1;
  
  if (DAT_20008099 == '\0') {
    iVar1 = 100;
  }
  else {
    iVar1 = 0x1e;
  }
  if (((iVar1 < g_reading) || (g_reading + iVar1 < 0 != SCARRY4(g_reading,iVar1))) ||
     (DAT_20008046 != '\0')) {
    LowZ_ResetQuietCount();
    if (DAT_20008099 == '\0') {
      g_reading = 0;
      DAT_2000023a = DAT_2000023a + -1;
      if (DAT_2000023a == '\0') {
        DAT_20008099 = '\x01';
        DAT_20008060 = 1;
      }
    }
  }
  else {
    g_reading = 0;
    DAT_20008099 = '\0';
    DAT_2000023a = '\x05';
    DAT_20008060 = 1;
    DAT_20000239 = DAT_20000239 + -1;
    if (DAT_20000239 == '\0') {
      DAT_20008098 = DAT_20008098 == '\0';
      LowZ_ResetQuietCount();
      Mode_ApplyChange(0);
    }
  }
  return;
}


// ==== LowZ_ResetQuietCount @ 0800662c

/* LowZ_AutoAcDc: quiet-display countdown 0x20000239 = 5 (re-arms the AC/DC toggle); tests
   0x20008098 but both branches store 5 (dead branch, harmless). high */

void LowZ_ResetQuietCount(void)

{
  DAT_20000239 = 5;
  return;
}


// ==== Mode_ACI_OnAD1 @ 0800664a

/* AC current modes 16/18/20. medium */

void Mode_ACI_OnAD1(void)

{
  if (g_cal_mode == '\0' && DAT_200080a5 == '\x01') {
    if (g_tdm_channel == 0) {
      Mode_ACI_Measure();
    }
    else {
      Mode_ACV_OnAD1();
      if (DAT_20008081 != '\0') {
        DAT_20007e6c = g_reading;
      }
    }
    if (g_display_update != '\0') {
      DAT_200080c5 = DAT_200080c5 + 1;
      if (DAT_200080c5 < 7) {
        DAT_2000805e = 0;
        g_display_update = '\0';
      }
      else {
        if (g_tdm_channel != 0) {
          DAT_20007e6c = Burden_Average(DAT_20007e6c);
          DAT_200080a7 = 5000 < DAT_20007e6c;
          DAT_2000805e = 0;
          g_display_update = '\0';
          if (g_setup_page == '\v') {
            DAT_20008062 = 1;
          }
        }
        DAT_200080c5 = 0;
        g_tdm_channel = g_tdm_channel ^ 1;
        Mode_ACI_SelectChannel(g_tdm_channel != 0);
      }
    }
  }
  else {
    Mode_ACI_Measure();
  }
  return;
}


// ==== Mode_ACI_SelectChannel @ 08006774

/* (burden) AC current TDM channel switch: 0 -> HY_SetMode/Cal_LoadRecord/Frontend_SetAll for
   g_mode/g_range; 1 -> the same for ACV range 0 (burden voltage); then Reading_SetSettleCount,
   clears 0x20007fc2. From Mode_ACI_OnAD1 and Setup_CurrentBurden. high */

void Mode_ACI_SelectChannel(char param_1)

{
  if (param_1 == '\0') {
    HY_SetMode(g_mode,g_range);
    Cal_LoadRecord(g_mode,g_range);
    Frontend_SetAll(g_mode,g_range);
  }
  else {
    HY_SetMode(2,0);
    Cal_LoadRecord(2,0);
    Frontend_SetAll(2,0);
  }
  Reading_SetSettleCount();
  DAT_20007fc2 = 0;
  return;
}


// ==== Mode_ACI_Measure @ 080067d6

/* AC current core (from Mode_ACI_OnAD1 and Mode_ACVA_OnAD1 I channel): on a new average
   (0x20008081): VA modes 13-15 -> 0800699a (Cal_Apply, clamp >= 0); else Cal_SubtractOffsetAC,
   clamp >= 0, Cal_DivideGain, ACFreq_Correct; cal mode: offset capture (0x2000808c -> 0x20007e08) /
   gain capture (0x2000808e); range 0 subtracts current-zero trim 0x20007e0c;
   Reading_StabilityFilterAC(20,10); cal-point capture 08007532; uA/mA/A range 0 below 31 counts ->
   0; per sample 08008468 + ACFreq_Correct on g_reading_fast; SETUP page 10 sets 0x20008062. high */

undefined4 Mode_ACI_Measure(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008081 != '\0') {
    if (((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) {
      VA_ACI_CalClamp();
    }
    else {
      if (DAT_2000808c == '\0') {
        if (((DAT_2000808e == '\0') || (g_mode == '\r')) ||
           ((g_mode == '\x0e' || (g_mode == '\x0f')))) {
          g_reading = Cal_SubtractOffsetAC(g_ad1_average);
          if (g_reading < 0) {
            g_reading = 0;
          }
          g_reading = Cal_DivideGain(g_reading);
          g_reading = ACFreq_Correct(g_mode,g_reading);
        }
        else {
          Cal_AcGainCapture(g_ad1_average,DAT_20007e08);
        }
      }
      else {
        DAT_20007e08 = Cal_AcOffsetCapture(g_ad1_average);
      }
      if ((g_range == '\0') && (g_reading = g_reading - DAT_20007e0c, g_reading < 0)) {
        g_reading = 0;
      }
    }
    g_reading = Reading_StabilityFilterAC(g_reading,0x14,10);
    Cal_AcFreqPointPoll();
    if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
       ((g_range == '\0' && (g_reading < 0x1f)))) {
      g_reading = 0;
    }
    g_display_update = 1;
    ACFreq_ClearOnLowSignal();
  }
  if (((g_mode != '\r') && (g_mode != '\x0e')) && (g_mode != '\x0f')) {
    Reading_FastPathAC(DAT_20007d74);
    g_reading_fast = ACFreq_Correct(g_mode,g_reading_fast);
  }
  if ((((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
       ((g_mode == '\x11' || (g_mode == '\x13')))) || (g_mode == '\x15')) && (g_setup_page == '\n'))
  {
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== VA_ACI_CalClamp @ 0800699a

/* Mode_ACI_Measure for AC VA modes 13-15 (I half): Cal_Apply, negative reading -> 0. high */

undefined4 VA_ACI_CalClamp(void)

{
  undefined4 unaff_r7;
  
  Cal_Apply();
  if (g_reading < 0) {
    g_reading = 0;
  }
  return unaff_r7;
}


// ==== Mode_ACmV_OnAD1 @ 080069b4

/* as ACV. high */

undefined4 Mode_ACmV_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if ((DAT_20008081 != '\0') && (DAT_200080b7 == '\0')) {
    if ((g_mode == '\r') || ((g_mode == '\x0e' || (g_mode == '\x0f')))) {
      VA_ACV_Measure();
    }
    else if (DAT_2000808c == '\0') {
      if (DAT_2000808e == '\0') {
        g_reading = Cal_SubtractOffsetAC(g_ad1_average);
        if (g_reading < 0) {
          g_reading = 0;
        }
        g_reading = Cal_DivideGain(g_reading);
        g_reading = ACFreq_LpfCompensate(g_reading);
        g_reading = ACFreq_Correct(g_mode,g_reading);
        g_reading = Reading_StabilityFilterAC(g_reading,0x14,10);
        Cal_AcFreqPointPoll();
        g_display_update = 1;
      }
      else {
        Cal_AcGainCapture(g_ad1_average,DAT_20007e08);
      }
    }
    else {
      DAT_20007e08 = Cal_AcOffsetCapture(g_ad1_average);
    }
  }
  Reading_FastPathAC(DAT_20007d74);
  g_reading_fast = ACFreq_Correct(g_mode,g_reading_fast);
  return unaff_r7;
}


// ==== VA_ACV_Measure @ 08006a9e

/* Mode_ACV_OnAD1/Mode_ACmV_OnAD1 in AC VA modes 13-15 (V half): Cal_Apply, clamp >= 0,
   Reading_StabilityFilterAC(20, 10), ACV_AfterReading. high */

undefined4 VA_ACV_Measure(void)

{
  undefined4 unaff_r7;
  
  Cal_Apply();
  if (g_reading < 0) {
    g_reading = 0;
  }
  g_reading = Reading_StabilityFilterAC(g_reading,0x14,10);
  ACV_AfterReading();
  return unaff_r7;
}


// ==== Mode_ACV_OnAD1 @ 08006aec

/* AD1 of the AD8436 output: AC offset, negatives -> 0, gain, filter (20, 10). high */

undefined4 Mode_ACV_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if ((DAT_20008081 != '\0') && (DAT_200080b7 == '\0')) {
    if ((g_mode == '\r') || ((g_mode == '\x0e' || (g_mode == '\x0f')))) {
      VA_ACV_Measure();
    }
    else if (DAT_2000808c == '\0') {
      if ((((DAT_2000808e == '\0') || (g_mode == '\r')) || (g_mode == '\x0e')) || (g_mode == '\x0f')
         ) {
        g_reading = Cal_SubtractOffsetAC(g_ad1_average);
        if (g_reading < 0) {
          g_reading = 0;
        }
        g_reading = Cal_DivideGain(g_reading);
        g_reading = ACFreq_LpfCompensate(g_reading);
        g_reading = ACFreq_Correct(g_mode,g_reading);
        g_reading = Reading_StabilityFilterAC(g_reading,0x14,10);
        Cal_AcFreqPointPoll();
        ACV_AfterReading();
      }
      else {
        Cal_AcGainCapture(g_ad1_average,DAT_20007e08);
      }
    }
    else {
      DAT_20007e08 = Cal_AcOffsetCapture(g_ad1_average);
    }
    ACFreq_ClearOnLowSignal();
  }
  if ((((((g_mode != '\r') && (g_mode != '\x0e')) &&
        ((g_mode != '\x0f' && ((g_mode != '\x10' && (g_mode != '\x12')))))) && (g_mode != '\x14'))
      && (((g_mode != '\x11' && (g_mode != '\x13')) && (g_mode != '\x15')))) &&
     (((g_mode != '\x02' || (DAT_200080ba == '\0')) &&
      ((g_mode != '\0' || (DAT_20000239 == '\x05')))))) {
    Reading_FastPathAC(DAT_20007d74);
    g_reading_fast = ACFreq_Correct(g_mode,g_reading_fast);
    if (((g_mode == '\x02') || (g_mode == '\x04')) && (g_lpf_1khz == '\x01')) {
      g_reading_fast = ACFreq_LpfCompensate(g_reading_fast * 10);
    }
  }
  return unaff_r7;
}


// ==== ACFreq_ClearOnLowSignal @ 08006c7a

/* ACV/ACmV/AC current on a display update (not ACV/ACmV with 1 kHz LPF): reading below 1000 counts
   (ACV range 3: 100) -> no-signal counter 0x20007fc0 = 2000, so the next tick's 2000-tick timeout
   zeroes the sub-display frequency (0x20007d84/0x2000803e). medium */

void ACFreq_ClearOnLowSignal(void)

{
  int iVar1;
  
  iVar1 = 0;
  if ((g_display_update != '\0') &&
     ((((g_mode != '\x02' && (g_mode != '\x04')) || (g_lpf_1khz != '\x01')) && (2 < DAT_20007fc0))))
  {
    if (g_mode == '\x02') {
      if (g_range == '\x03') {
        iVar1 = 100;
      }
      else {
        iVar1 = 1000;
      }
    }
    else if (((g_mode == '\x04') || (g_mode == '\x10')) ||
            ((g_mode == '\x12' || (g_mode == '\x14')))) {
      iVar1 = 1000;
    }
    if ((iVar1 != 0) && (g_reading < iVar1)) {
      DAT_20007fc0 = 2000;
    }
  }
  return;
}


// ==== ACFreq_LpfCompensate @ 08006d40  [GPIOE]

/* ACV/ACmV with g_lpf_1khz only: f = Freq_ToDeciHz(0x2000803e, 0x20007d84); f < 20 unchanged, f >
   6000 -> reading 0; else reading * k / 10000, k interpolated in f over 216 points (freq
   0x0802501c, 20..6000) and in amplitude between a low table 0x080256dc (reading < 100,000) and a
   high table 0x0802537c (> 500,000; linear between). k rises 1015 at f=20 to ~3815 at f=820, then
   falls to ~729 at 4000 (the x10 LPF scaling folded in). Index 0 reads the word before the table
   (harmless: ratio 1.0). medium */

int ACFreq_LpfCompensate(int param_1)

{
  undefined4 uVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined8 uVar6;
  
  if ((((g_mode == '\x02') || (g_mode == '\x04')) && (g_lpf_1khz == '\x01')) &&
     (DAT_20007c5c = Freq_ToDeciHz(DAT_2000803e,DAT_20007d84), 0x13 < DAT_20007c5c)) {
    if (DAT_20007c5c < 0x1771) {
      for (iVar5 = 0; iVar3 = 0xd7, iVar5 < 0xd8; iVar5 = iVar5 + 1) {
        if (DAT_20007c5c <= (int)(&DAT_0802501c)[iVar5]) {
          iVar3 = iVar5;
          break;
        }
      }
      DAT_20007c74 = (&DAT_0802501c)[iVar3];
      DAT_20007c78 = (&PTR_GPIOE_BSRR_2_08025018)[iVar3];
      DAT_20007c6c = DAT_20007c74 - (int)DAT_20007c78;
      DAT_20007c70 = DAT_20007c5c - (int)DAT_20007c78;
      uVar1 = __aeabi_i2f(DAT_20007c70);
      uVar2 = __aeabi_i2f(DAT_20007c6c);
      DAT_20007c84 = __aeabi_fdiv(uVar1,uVar2);
      if (param_1 < 100000) {
        DAT_20007c74 = (&DAT_080256dc)[iVar3];
        iVar5 = (&DAT_080256d8)[iVar3];
        DAT_20007c70 = DAT_20007c74 - iVar5;
        DAT_20007c6c = iVar5;
        DAT_20007c78 = (undefined *)iVar5;
        uVar1 = __aeabi_i2f(DAT_20007c70);
        __aeabi_fmul(uVar1,DAT_20007c84);
        uVar6 = __aeabi_f2d();
        __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
        iVar3 = __aeabi_d2iz();
        DAT_20007c60 = iVar3 + iVar5;
      }
      else if (param_1 < 0x7a121) {
        DAT_20007c74 = (&DAT_080256dc)[iVar3];
        iVar4 = (&DAT_080256d8)[iVar3];
        DAT_20007c7c = (&DAT_0802537c)[iVar3];
        DAT_20007c80 = (&DAT_08025378)[iVar3];
        DAT_20007c70 = DAT_20007c74 - iVar4;
        DAT_20007c6c = iVar4;
        DAT_20007c78 = (undefined *)iVar4;
        uVar1 = __aeabi_i2f(DAT_20007c70);
        __aeabi_fmul(uVar1,DAT_20007c84);
        uVar6 = __aeabi_f2d();
        __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
        iVar3 = __aeabi_d2iz();
        iVar5 = DAT_20007c80;
        DAT_20007c64 = iVar3 + iVar4;
        DAT_20007c6c = DAT_20007c80;
        DAT_20007c70 = DAT_20007c7c - DAT_20007c80;
        uVar1 = __aeabi_i2f(DAT_20007c70);
        __aeabi_fmul(uVar1,DAT_20007c84);
        uVar6 = __aeabi_f2d();
        __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
        iVar3 = __aeabi_d2iz();
        DAT_20007c68 = iVar3 + iVar5;
        uVar1 = __aeabi_i2f(param_1);
        uVar1 = __aeabi_fadd(uVar1,0xc7c35000);
        uVar1 = __aeabi_fdiv(uVar1,0x48c35000);
        uVar2 = __aeabi_i2f(DAT_20007c68 - DAT_20007c64);
        DAT_20007c88 = __aeabi_fmul(uVar2,uVar1);
        iVar5 = DAT_20007c64;
        iVar3 = __aeabi_f2iz(DAT_20007c88);
        DAT_20007c60 = iVar3 + iVar5;
      }
      else {
        DAT_20007c74 = (&DAT_0802537c)[iVar3];
        iVar5 = (&DAT_08025378)[iVar3];
        DAT_20007c70 = DAT_20007c74 - iVar5;
        DAT_20007c6c = iVar5;
        DAT_20007c78 = (undefined *)iVar5;
        uVar1 = __aeabi_i2f(DAT_20007c70);
        __aeabi_fmul(uVar1,DAT_20007c84);
        uVar6 = __aeabi_f2d();
        __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
        iVar3 = __aeabi_d2iz();
        DAT_20007c60 = iVar3 + iVar5;
      }
      param_1 = (DAT_20007c60 * param_1) / 10000;
    }
    else {
      param_1 = 0;
    }
  }
  return param_1;
}


// ==== ACFreq_CorrectAcv0 @ 080070cc  [DAT]

/* (reading, freq count, freq range) -> correction counts to subtract; only ACV range 0 on
   g_variant_acv0 meters (EEPROM 0x1F != 0xA5), via ACFreq_Correct. reading >= 57000 or f <= 50 ->
   0. Interpolates two curves in f (17 points 0x08028c58: 50..100000) from tables 0x08028648 /
   0x0802868c (reading <= 5000: 0..first), then blends by a percentage of reading around (low curve
   + 5000) toward 50,000. Never executed. medium */

undefined8 ACFreq_CorrectAcv0(int param_1,undefined4 param_2,undefined1 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined8 uVar6;
  
  DAT_20007c8c = 0;
  if ((param_1 < 57000) && (DAT_20007c90 = Freq_ToDeciHz(param_3,param_2), 0x32 < DAT_20007c90)) {
    for (iVar5 = 0; iVar4 = 0x10, iVar5 < 0x11; iVar5 = iVar5 + 1) {
      if (DAT_20007c90 <= (int)(&DAT_08028c58)[iVar5]) {
        iVar4 = iVar5;
        break;
      }
    }
    DAT_20007cac = (&DAT_08028c58)[iVar4];
    DAT_20007cb0 = (&DAT_08028c54)[iVar4];
    DAT_20007c9c = DAT_20007cac - DAT_20007cb0;
    DAT_20007ca0 = DAT_20007c90 - DAT_20007cb0;
    uVar1 = __aeabi_i2f(DAT_20007ca0);
    uVar2 = __aeabi_i2f(DAT_20007c9c);
    DAT_20007cbc = __aeabi_fdiv(uVar1,uVar2);
    if (param_1 < 0x1389) {
      DAT_20007cac = 0;
      DAT_20007cb0 = 0;
      DAT_20007cb4 = (&DAT_08028648)[iVar4];
      DAT_20007cb8 = (&DAT_08028644)[iVar4];
    }
    else {
      DAT_20007cac = (&DAT_08028648)[iVar4];
      DAT_20007cb0 = (&DAT_08028644)[iVar4];
      DAT_20007cb4 = (&DAT_0802868c)[iVar4];
      DAT_20007cb8 = (&DAT_08028688)[iVar4];
    }
    iVar5 = DAT_20007cb0;
    DAT_20007c9c = DAT_20007cb0;
    DAT_20007ca0 = DAT_20007cac - DAT_20007cb0;
    uVar1 = __aeabi_i2f(DAT_20007ca0);
    __aeabi_fmul(uVar1,DAT_20007cbc);
    uVar6 = __aeabi_f2d();
    __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
    iVar3 = __aeabi_d2iz();
    iVar4 = DAT_20007cb8;
    DAT_20007c94 = iVar3 + iVar5;
    DAT_20007c9c = DAT_20007cb8;
    DAT_20007ca0 = DAT_20007cb4 - DAT_20007cb8;
    uVar1 = __aeabi_i2f(DAT_20007ca0);
    __aeabi_fmul(uVar1,DAT_20007cbc);
    uVar6 = __aeabi_f2d();
    __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
    iVar5 = __aeabi_d2iz();
    DAT_20007c98 = iVar5 + iVar4;
    DAT_20007ca8 = DAT_20007c94 + 5000;
    if (DAT_20007ca8 < param_1) {
      uVar1 = __aeabi_i2f(param_1);
      uVar2 = __aeabi_i2f(DAT_20007ca8);
      uVar1 = __aeabi_fsub(uVar1,uVar2);
      uVar2 = __aeabi_i2f(DAT_20007ca8);
      uVar2 = __aeabi_fsub(0x47435000,uVar2);
      uVar1 = __aeabi_fdiv(uVar1,uVar2);
      __aeabi_fmul(uVar1,&DAT_42c80000);
      DAT_20007ca4 = __aeabi_f2iz();
      DAT_20007cac = DAT_20007c98 - DAT_20007c94;
      DAT_20007cb0 = DAT_20007cac - (DAT_20007ca4 * DAT_20007cac) / 100;
      DAT_20007cb4 = DAT_20007c98 - DAT_20007cb0;
    }
    else {
      uVar1 = __aeabi_i2f(DAT_20007ca8);
      uVar2 = __aeabi_i2f(param_1);
      uVar1 = __aeabi_fsub(uVar1,uVar2);
      uVar2 = __aeabi_i2f(DAT_20007ca8);
      uVar1 = __aeabi_fdiv(uVar1,uVar2);
      __aeabi_fmul(uVar1,&DAT_42c80000);
      DAT_20007ca4 = __aeabi_f2iz();
      DAT_20007cac = DAT_20007c98 - DAT_20007c94;
      DAT_20007cb0 = DAT_20007cac - (DAT_20007ca4 * DAT_20007cac) / 100;
      DAT_20007cb4 = DAT_20007c98 - DAT_20007cb0;
    }
    DAT_20007c8c = DAT_20007cb4;
  }
  return CONCAT44(param_4,DAT_20007c8c);
}


// ==== ACFreq_Correct @ 0800742c

/* (mode, reading) -> reading - AC frequency-response correction; only when current burden display
   0x200080a5 off, not ACV/ACmV with 1 kHz LPF, not cal mode, and mode is ACmV/ACV/AC uA/mA/A. Table
   index: ACmV range (0-1), ACV range + 2 (2-5), 16/18/20 -> 6/7/8 (mode 0 -> 5 is dead code);
   080075fa -> 080124f4 (freq tables + cal points 0x20007e4c..58; rejects index >= 8, so AC A gets
   none). Then ACFreq_CorrectAcv0 for ACV range 0 on variant meters. Result also kept in 0x20007e5c.
   medium */

undefined8 ACFreq_Correct(char param_1,int param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  
  cVar1 = '\0';
  if ((((DAT_200080a5 == '\0') &&
       (((param_1 != '\x02' && (param_1 != '\x04')) || (g_lpf_1khz != '\x01')))) &&
      (g_cal_mode == '\0')) &&
     (((param_1 == '\x10' || (param_1 == '\x12')) ||
      ((param_1 == '\x14' || ((param_1 == '\x02' || (param_1 == '\x04')))))))) {
    if (param_1 == '\x04') {
      cVar1 = g_range;
    }
    else if (param_1 == '\x02') {
      cVar1 = g_range + '\x02';
    }
    else if (param_1 == '\0') {
      cVar1 = '\x05';
    }
    else if (((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) {
      if (param_1 == '\x10') {
        cVar1 = '\x06';
      }
      else if (param_1 == '\x12') {
        cVar1 = '\a';
      }
      else {
        cVar1 = '\b';
      }
    }
    DAT_20007e5c = ACFreq_ComputeCorrection(cVar1,param_2);
    param_2 = param_2 - DAT_20007e5c;
    if (((g_variant_acv0 != '\0') && (param_1 == '\x02')) && (g_range == '\0')) {
      DAT_20007e5c = ACFreq_CorrectAcv0(param_2,DAT_20007d84,DAT_2000803e);
      param_2 = param_2 - DAT_20007e5c;
    }
  }
  return CONCAT44(param_4,param_2);
}


// ==== Cal_AcFreqPointPoll @ 08007532

/* calibration mode, ACmV/ACV/AC current, burden display off: each pending AC frequency-point flag
   0x2000809c..9f (set by Cal_AcFreqWindow) -> clear it, Cal_AcFreqPointStore(1..4, g_reading),
   Beep_Short. high */

undefined4 Cal_AcFreqPointPoll(void)

{
  undefined4 unaff_r7;
  
  if (((DAT_200080a5 == '\0') && (g_cal_mode != '\0')) &&
     (((g_mode == '\x10' || (((g_mode == '\x12' || (g_mode == '\x14')) || (g_mode == '\x02')))) ||
      (g_mode == '\x04')))) {
    if (DAT_2000809c != '\0') {
      DAT_2000809c = '\0';
      Cal_AcFreqPointStore(1,g_reading);
      Beep_Short();
    }
    if (DAT_2000809d != '\0') {
      DAT_2000809d = '\0';
      Cal_AcFreqPointStore(2,g_reading);
      Beep_Short();
    }
    if (DAT_2000809e != '\0') {
      DAT_2000809e = '\0';
      Cal_AcFreqPointStore(3,g_reading);
      Beep_Short();
    }
    if (DAT_2000809f != '\0') {
      DAT_2000809f = '\0';
      Cal_AcFreqPointStore(4,g_reading);
      Beep_Short();
    }
  }
  return unaff_r7;
}


// ==== ACFreq_ComputeCorrection @ 080075fa

/* (table index, reading) from ACFreq_Correct: picks the frequency source (ACV with AC+DC
   0x200080ba: 0x20007e60/0x200080a0; LowZ in AC 0x20008098: 0x20007e64/0x200080a1; else the current
   0x20007d84/0x2000803e) and returns AcFreq_Correction(reading, cal points 0x20007e4c..58, index,
   freq, freq range, g_variant_acv0). high */

undefined4 ACFreq_ComputeCorrection(undefined1 param_1,undefined4 param_2)

{
  undefined4 uVar1;
  
  if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
     ((g_mode == '\x02' || (g_mode == '\x04')))) {
    if ((g_mode == '\x02') && (DAT_200080ba != '\0')) {
      DAT_20007d84 = DAT_20007e60;
      DAT_2000803e = DAT_200080a0;
    }
  }
  else if ((g_mode == '\0') && (DAT_20008098 != '\0')) {
    DAT_20007d84 = DAT_20007e64;
    DAT_2000803e = DAT_200080a1;
  }
  uVar1 = AcFreq_Correction(param_2,DAT_20007e4c,DAT_20007e50,DAT_20007e54,DAT_20007e58,param_1,
                            DAT_20007d84,DAT_2000803e,g_variant_acv0);
  return uVar1;
}


// ==== ACV_AfterReading @ 080076bc

/* end of the ACV handler (also 08006a9e): with AC+DC (0x200080ba) the reading feeds ACDC_Step(0)
   while in the AC phase (0x200080bd == 0) instead of a display update, else g_display_update = 1;
   then ACV_ComputeDbm; SETUP page 11 -> refresh flag 0x20008062. high */

undefined4 ACV_AfterReading(void)

{
  undefined4 unaff_r7;
  
  if ((g_mode == '\x02') && (DAT_200080ba != '\0')) {
    if (DAT_200080bd == '\0') {
      ACDC_Step(0);
    }
  }
  else {
    g_display_update = 1;
  }
  ACV_ComputeDbm();
  if ((g_mode == '\x02') && (g_setup_page == '\v')) {
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== Mode_DCV_OnAD1 @ 08007714

/* DCV: on average -> 08006418 (cal), 0800b754, display flag 0x2000805d; per sample 080064d6. high
    */

undefined4 Mode_DCV_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008081 != '\0') {
    Cal_Apply();
    g_reading = Reading_StabilityFilter(g_reading,2,2);
    if ((g_mode != '\x02') || (DAT_200080ba == '\0')) {
      g_display_update = 1;
    }
    if (((g_mode == '\x02') && (DAT_200080ba != '\0')) && (DAT_200080bd == '\x01')) {
      ACDC_Step(1);
    }
  }
  if (((((g_mode != '\x02') || (DAT_200080ba == '\0')) &&
       ((g_mode != '\x10' && ((g_mode != '\x12' && (g_mode != '\x14')))))) && (g_mode != '\x11')) &&
     (((g_mode != '\x13' && (g_mode != '\x15')) && ((g_mode != '\0' || (DAT_20000239 == '\x05'))))))
  {
    Reading_FastPath();
  }
  return unaff_r7;
}


// ==== Mode_DCmV_OnAD1 @ 080077f0

/* cal, filter (5, 10). high */

undefined4 Mode_DCmV_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008081 != '\0') {
    Cal_Apply();
    g_reading = Reading_StabilityFilter(g_reading,5,10);
    g_display_update = 1;
  }
  Reading_FastPath();
  return unaff_r7;
}


// ==== Mode_Temp_OnAD1 @ 08007840

/* medium */

void Mode_Temp_OnAD1(void)

{
  int iVar1;
  
  Mode_DCmV_OnAD1();
  if (g_display_update == '\0') {
    DAT_2000805e = 0;
  }
  else {
    TC_EmfToTemp(DAT_20007e7c + g_reading);
    iVar1 = DAT_20007e80;
    if (g_temp_unit == '\0') {
      iVar1 = Temp_CToF(DAT_20007e80);
    }
    g_reading_fast = iVar1 / 10;
    if (4 < g_reading_fast % 10) {
      g_reading_fast = g_reading_fast + 1;
    }
    if (g_display_update != '\0') {
      g_reading = g_reading_fast;
    }
  }
  return;
}


// ==== Mode_Continuity_OnAD1 @ 080078b8

/* Ohm handler + beeper on the fast reading. high */

undefined4 Mode_Continuity_OnAD1(void)

{
  undefined4 unaff_r7;
  
  Mode_Ohm_OnAD1();
  Continuity_BeepDecide(g_reading_fast);
  return unaff_r7;
}


// ==== Continuity_BeepDecide @ 080078d8

/* (fast reading) from Mode_Continuity_OnAD1: g_continuity_mode 0/2 (DN 30/300) beep while reading <
   Continuity_Threshold, 1/3 (UP) while reading >= it; beep state 0x200080b5, tone via 0800eed0(100)
   each sample. high */

undefined4
Continuity_BeepDecide(uint param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  uVar1 = Continuity_Threshold();
  if ((g_continuity_mode == '\0') || (g_continuity_mode == '\x02')) {
    if (uVar1 < param_1) {
      DAT_200080b5 = 0;
    }
    else {
      DAT_200080b5 = 1;
      Buzzer_OnAlways(100);
    }
  }
  else if (param_1 < uVar1) {
    DAT_200080b5 = 0;
  }
  else {
    DAT_200080b5 = 1;
    Buzzer_OnAlways(100);
  }
  return param_4;
}


// ==== Continuity_Threshold @ 08007940

/* beep threshold with 500-count hysteresis by g_continuity_mode and beep state 0x200080b5: off
   3000/3000/30000/30000; on 3500 (DN30), 2500 (UP30), 30500 (DN300), 29500 (UP300). high (emulated)
    */

undefined4 Continuity_Threshold(void)

{
  undefined4 uVar1;
  
  uVar1 = 3000;
  if (DAT_200080b5 == '\0') {
    if (g_continuity_mode < 2) {
      uVar1 = 3000;
    }
    else if (g_continuity_mode - 2 < 2) {
      uVar1 = 30000;
    }
  }
  else if (g_continuity_mode == 0) {
    uVar1 = 0xdac;
  }
  else if (g_continuity_mode == 2) {
    uVar1 = 0x7724;
  }
  else if (g_continuity_mode < 2) {
    uVar1 = 0x9c4;
  }
  else if (g_continuity_mode == 3) {
    uVar1 = 0x733c;
  }
  return uVar1;
}


// ==== Mode_Ohm_OnAD1 @ 080079c4

/* medium */

void Mode_Ohm_OnAD1(void)

{
  int iVar1;
  int iVar2;
  undefined4 uVar3;
  undefined2 uVar4;
  
  if (DAT_20008081 != '\0') {
    Cal_Apply();
    if ((g_reading < 0) && (g_range != 0)) {
      g_reading = 0;
    }
    if (g_range == 6) {
      uVar3 = 100;
      uVar4 = 0x14;
    }
    else if (g_range == 5) {
      uVar3 = 10;
      uVar4 = 5;
    }
    else if (g_range == 0) {
      uVar3 = 100;
      uVar4 = 10;
    }
    else if ((g_range < 2) || (3 < g_range)) {
      uVar3 = 0x14;
      uVar4 = 10;
    }
    else if (g_range == 2) {
      uVar3 = 10;
      uVar4 = 10;
    }
    else if (g_range == 3) {
      uVar3 = 0x14;
      uVar4 = 10;
    }
    else {
      uVar3 = 10;
      uVar4 = 5;
    }
    iVar1 = Reading_StabilityFilter(g_reading,uVar3,uVar4);
    g_reading = iVar1;
    if (g_variant_ohm6 == '\0') {
      if (((g_cal_mode == '\0') && (g_mode == '\t')) && (g_range == 5)) {
        iVar2 = Ohm_R5Correction(iVar1);
        g_reading = iVar1 - iVar2;
      }
      iVar1 = g_reading;
      if (((g_cal_mode == '\0') && (g_mode == '\t')) && (g_range == 6)) {
        iVar2 = Ohm_R6Correction(g_reading,DAT_20007e14);
        g_reading = iVar1 - iVar2;
      }
    }
    g_display_update = 1;
  }
  Reading_FastPath();
  if (g_reading_fast < 0) {
    g_reading_fast = 0;
  }
  return;
}


// ==== Ohm_R5Correction @ 08007b70

/* Ohm range 5 linearity trim subtracted by Mode_Ohm_OnAD1 (only when g_variant_ohm6 == 0, not cal
   mode): triangular 0..15 counts, +1 per 1000 counts up to 15 at 15,000..35,999, back down to 1 at
   49,999; >= 50,000 -> 0. high (emulated: emu_call) */

undefined4 Ohm_R5Correction(int param_1)

{
  undefined4 uVar1;
  
  uVar1 = 0;
  if (param_1 < 50000) {
    if (param_1 < 1000) {
      uVar1 = 0;
    }
    else if (param_1 < 2000) {
      uVar1 = 1;
    }
    else if (param_1 < 3000) {
      uVar1 = 2;
    }
    else if (param_1 < 4000) {
      uVar1 = 3;
    }
    else if (param_1 < 5000) {
      uVar1 = 4;
    }
    else if (param_1 < 6000) {
      uVar1 = 5;
    }
    else if (param_1 < 7000) {
      uVar1 = 6;
    }
    else if (param_1 < 8000) {
      uVar1 = 7;
    }
    else if (param_1 < 9000) {
      uVar1 = 8;
    }
    else if (param_1 < 10000) {
      uVar1 = 9;
    }
    else if (param_1 < 11000) {
      uVar1 = 10;
    }
    else if (param_1 < 12000) {
      uVar1 = 0xb;
    }
    else if (param_1 < 13000) {
      uVar1 = 0xc;
    }
    else if (param_1 < 14000) {
      uVar1 = 0xd;
    }
    else if (param_1 < 15000) {
      uVar1 = 0xe;
    }
    else if (param_1 < 36000) {
      uVar1 = 0xf;
    }
    else if (param_1 < 37000) {
      uVar1 = 0xe;
    }
    else if (param_1 < 38000) {
      uVar1 = 0xd;
    }
    else if (param_1 < 39000) {
      uVar1 = 0xc;
    }
    else if (param_1 < 40000) {
      uVar1 = 0xb;
    }
    else if (param_1 < 41000) {
      uVar1 = 10;
    }
    else if (param_1 < 42000) {
      uVar1 = 9;
    }
    else if (param_1 < 43000) {
      uVar1 = 8;
    }
    else if (param_1 < 44000) {
      uVar1 = 7;
    }
    else if (param_1 < 45000) {
      uVar1 = 6;
    }
    else if (param_1 < 46000) {
      uVar1 = 5;
    }
    else if (param_1 < 47000) {
      uVar1 = 4;
    }
    else if (param_1 < 48000) {
      uVar1 = 3;
    }
    else if (param_1 < 49000) {
      uVar1 = 2;
    }
    else if (param_1 < 50000) {
      uVar1 = 1;
    }
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}


// ==== Ohm_R6Correction @ 08007d1e  [DAT]

/* (reading, trim 0x20007e14) -> counts subtracted on Ohm range 6 (g_variant_ohm6 == 0): table
   0x08028868 (0,37,65,85,100,100,100,87,70,40,0,-37,-65) interpolated at 5,000-count steps
   (0x08028838), times trim / 100; reading >= 56,000 -> 0. Trim = EEPROM 0x178 (u32, +-3000 else 0),
   captured in cal as avg/gain - 30000 (0800f470). Reading 0 indexes the word before the table. high
   (emulated: emu_call 20000,100 -> 100; 55999 -> -42) */

undefined4 Ohm_R6Correction(int param_1,undefined4 param_2)

{
  undefined4 uVar1;
  undefined4 uVar2;
  uint uVar3;
  uint uVar4;
  uint unaff_r7;
  
  DAT_20007cc0 = 0;
  if (param_1 < 56000) {
    uVar3 = 0;
    while ((uVar4 = unaff_r7, (uVar3 & 0xff) < 0xd &&
           (uVar4 = uVar3 & 0xff, (&DAT_08028838)[uVar4] * 1000 < param_1))) {
      uVar3 = uVar4 + 1;
    }
    DAT_20007ccc = (&DAT_08028834)[uVar4 & 0xff];
    DAT_20007cd0 = (&DAT_08028838)[uVar4 & 0xff];
    uVar1 = __aeabi_i2f(param_1 + DAT_20007ccc * -1000);
    uVar2 = __aeabi_i2f((DAT_20007cd0 - DAT_20007ccc) * 1000);
    DAT_20007cc4 = __aeabi_fdiv(uVar1,uVar2);
    DAT_20007cc8 = __aeabi_i2f((&DAT_0802886c)[uVar4 & 0xff] - (&DAT_08028868)[uVar4 & 0xff]);
    uVar1 = __aeabi_fmul(DAT_20007cc4,DAT_20007cc8);
    uVar2 = __aeabi_i2f((&DAT_08028868)[uVar4 & 0xff]);
    DAT_20007cc8 = __aeabi_fadd(uVar2,uVar1);
    uVar1 = __aeabi_i2f(param_2);
    uVar1 = __aeabi_fmul(uVar1,DAT_20007cc8);
    __aeabi_fdiv(uVar1,&DAT_42c80000);
    DAT_20007cc0 = __aeabi_f2iz();
  }
  else {
    DAT_20007cc0 = 0;
  }
  return DAT_20007cc0;
}


// ==== Mode_Diode_OnAD1 @ 08007e84

/* 15 V: /10; variant +500 (PF8 strap low, 15 V, > 14000); beeps. high */

undefined4 Mode_Diode_OnAD1(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008081 != '\0') {
    Cal_Apply();
    if (g_diode_15v != '\0') {
      g_reading = g_reading / 10;
    }
    g_reading = Reading_StabilityFilter(g_reading,2,2);
    if (((g_cal_mode == '\0' && g_pf8_strap_low == '\x01') && g_diode_15v == '\x01') &&
       (14000 < g_reading)) {
      g_reading = g_reading + 500;
    }
    if ((g_reading < 1000) && (-1000 < g_reading)) {
      Buzzer_OnAlways(500);
      DAT_20007cd4 = g_reading;
    }
    else if (((DAT_20007cd4 + 1000 < g_reading) || (g_reading < DAT_20007cd4 + -1000)) &&
            (DAT_200080b4 == '\0')) {
      DAT_20007cd4 = g_reading;
      DAT_200080b4 = 'd';
      Beep_Double();
    }
    g_display_update = 1;
  }
  Reading_FastPath();
  if (g_diode_15v != '\0') {
    g_reading_fast = g_reading_fast / 10;
  }
  return unaff_r7;
}


// ==== Mode_DCI_OnAD1 @ 08007fa8

/* DC current modes 17/19/21. medium */

void Mode_DCI_OnAD1(void)

{
  if (g_cal_mode == '\0' && DAT_200080a5 == '\x01') {
    if (g_tdm_channel == 0) {
      Mode_DCI_Measure();
    }
    else {
      Mode_DCV_OnAD1();
      if (DAT_20008081 != '\0') {
        DAT_20007e6c = g_reading;
      }
    }
    if (g_display_update != '\0') {
      DAT_200080c5 = DAT_200080c5 + 1;
      if (DAT_200080c5 < 4) {
        DAT_2000805e = 0;
        g_display_update = '\0';
      }
      else {
        if (g_tdm_channel != 0) {
          DAT_20007e6c = Burden_Average(DAT_20007e6c);
          DAT_200080a7 = 5000 < DAT_20007e6c;
          DAT_2000805e = 0;
          g_display_update = '\0';
          if (g_setup_page == '\v') {
            DAT_20008062 = 1;
          }
        }
        DAT_200080c5 = 0;
        g_tdm_channel = g_tdm_channel ^ 1;
        Mode_DCI_SelectChannel(g_tdm_channel != 0);
      }
    }
  }
  else {
    Mode_DCI_Measure();
  }
  return;
}


// ==== Mode_DCI_SelectChannel @ 080080c8

/* (burden) DC current TDM channel switch: 0 -> HY_SetMode/Cal_LoadRecord/Frontend_SetAll for
   g_mode/g_range; 1 -> DCV range 0 (burden voltage); then Reading_SetSettleCount. From
   Mode_DCI_OnAD1 and Setup_CurrentBurden. high */

undefined4
Mode_DCI_SelectChannel(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  if (param_1 == '\0') {
    HY_SetMode(g_mode,g_range);
    Cal_LoadRecord(g_mode,g_range);
    Frontend_SetAll(g_mode,g_range);
  }
  else {
    HY_SetMode(1,0);
    Cal_LoadRecord(1,0);
    Frontend_SetAll(1,0);
  }
  Reading_SetSettleCount();
  return param_4;
}


// ==== Mode_DCI_Measure @ 08008138

/* DC current core (Mode_DCI_OnAD1, Mode_DCVA_OnAD1 I channel): on a new average (0x20008081)
   Cal_Apply, Reading_StabilityFilter(10,10), + DCVA_CurrentSignTrim(g_va_volts, reading), range 0
   minus current-zero trim 0x20007e0c, display update; Reading_FastPath; SETUP page 10 in current
   modes -> 0x20008062. high */

void Mode_DCI_Measure(void)

{
  int iVar1;
  
  if (DAT_20008081 != '\0') {
    Cal_Apply();
    g_reading = Reading_StabilityFilter(g_reading,10,10);
    iVar1 = DCVA_CurrentSignTrim(g_va_volts,g_reading);
    g_reading = iVar1 + g_reading;
    if (g_range == '\0') {
      g_reading = g_reading - DAT_20007e0c;
    }
    g_display_update = 1;
  }
  Reading_FastPath();
  if (((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
      (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))) && (g_setup_page == '\n'))
  {
    DAT_20008062 = 1;
  }
  return;
}


// ==== HY_OnRMS @ 08008208

/* RMS ready (RMSF). medium */

undefined4 HY_OnRMS(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  DAT_20007d78 = HY_RmsRead();
  DAT_20007de0 = DAT_20007d78;
  iVar1 = HY_RmsAverage4(DAT_20007d78);
  DAT_20008082 = iVar1 != 0;
  if (g_mode == '\0') {
    HY_RMS_DiscardLowZ();
  }
  else if (g_mode == '\x02') {
    HY_RMS_Discard();
  }
  else if (g_mode == '\x04') {
    HY_RMS_NopACmV();
  }
  else if (((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) {
    HY_RMS_NopACI();
  }
  return unaff_r7;
}


// ==== HY_RmsRead @ 0800827c

/* from HY_OnRMS: 32-bit value from the RMS registers (0x200077da..dd, byte 0x200077d9 kept in
   0x20007dd8) -> 0x20007ddc, returns (int)sqrt(x) * 8 (to 0x20007d78); result unused (HY RMS
   discarded, modes.md). medium */

int HY_RmsRead(void)

{
  int iVar1;
  
  DAT_20007ddc = (uint)DAT_200077da +
                 (uint)DAT_200077dc * 0x10000 + (uint)DAT_200077dd * 0x1000000 +
                 (uint)DAT_200077db * 0x100;
  DAT_20007dd8 = (uint)DAT_200077d9;
  __aeabi_ui2d(DAT_20007ddc);
  sqrt();
  iVar1 = __aeabi_d2iz();
  return iVar1 << 3;
}


// ==== HY_RmsAverage4 @ 080082d4

/* (value) from HY_OnRMS: sums 4 values (count 0x20008083, sum 0x20007de4); on the 4th stores the
   mean in 0x20007de8, resets, returns true. medium */

bool HY_RmsAverage4(uint param_1)

{
  bool bVar1;
  
  if (DAT_20008083 == '\0') {
    DAT_20007de4 = param_1;
  }
  else {
    DAT_20007de4 = param_1 + DAT_20007de4;
  }
  DAT_20008083 = DAT_20008083 + 1;
  bVar1 = 3 < DAT_20008083;
  if (bVar1) {
    DAT_20007de8 = DAT_20007de4 / DAT_20008083;
    DAT_20008083 = 0;
    DAT_20007de4 = 0;
  }
  return bVar1;
}


// ==== Reading_StabilityFilterAC @ 08008344

/* (value, threshold, n): same algorithm as Reading_StabilityFilter (running mean of up to n,
   restart on a jump > threshold) with its own state: count 0x20008084, history 0x200070a8, mean
   0x20007cd8, 64-bit sum / __aeabi_ldivmod. Used by the AC V/mV/I handlers with (20, 10). high */

int Reading_StabilityFilterAC(int param_1,int param_2,ushort param_3)

{
  short sVar1;
  int iVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  bool bVar6;
  
  uVar4 = 0;
  iVar5 = 0;
  iVar2 = param_1 - DAT_20007cd8;
  if ((param_2 < iVar2) || (iVar2 + param_2 < 0 != SCARRY4(iVar2,param_2))) {
    DAT_20008084 = '\0';
  }
  DAT_200070a8 = param_1;
  if (DAT_20008084 == '\0') {
    DAT_20008084 = 1;
    DAT_20007cd8 = param_1;
  }
  else {
    DAT_20008084 = DAT_20008084 + 1;
    if (param_3 < DAT_20008084) {
      DAT_20008084 = (byte)param_3;
    }
    iVar2 = 1;
    while (sVar1 = (short)iVar2, sVar1 < (short)(ushort)DAT_20008084) {
      (&DAT_200070a8)[(uint)DAT_20008084 - (int)sVar1] =
           (&DAT_200070a8)[(DAT_20008084 - 1) - (int)sVar1];
      iVar2 = sVar1 + 1;
    }
    iVar2 = 0;
    while (sVar1 = (short)iVar2, sVar1 < (short)(ushort)DAT_20008084) {
      uVar3 = (&DAT_200070a8)[sVar1];
      bVar6 = CARRY4(uVar4,uVar3);
      uVar4 = uVar4 + uVar3;
      iVar5 = iVar5 + ((int)uVar3 >> 0x1f) + (uint)bVar6;
      iVar2 = sVar1 + 1;
    }
    DAT_20007cd8 = __aeabi_ldivmod(uVar4,iVar5,DAT_20008084,0);
  }
  return DAT_20007cd8;
}


// ==== Reading_FastPathAC @ 08008468

/* (raw) per-sample fast AC reading: Cal_SubtractOffsetAC, Cal_DivideGain, /10 for ACV/ACmV with 1
   kHz LPF -> g_reading_fast, flag 0x2000805e. high */

void Reading_FastPathAC(undefined4 param_1)

{
  g_reading_fast = Cal_SubtractOffsetAC(param_1);
  g_reading_fast = Cal_DivideGain(g_reading_fast);
  if (((g_mode == '\x02') || (g_mode == '\x04')) && (g_lpf_1khz == '\x01')) {
    g_reading_fast = g_reading_fast / 10;
  }
  DAT_2000805e = 1;
  return;
}


// ==== Mode_DCVA_OnAD1 @ 080084e0

/* time-shared V / I with the DCV handler. high */

void Mode_DCVA_OnAD1(void)

{
  byte bVar1;
  bool bVar2;
  
  DAT_200080c3 = '\0';
  if (g_tdm_channel == 0) {
    Mode_DCV_OnAD1();
    if (DAT_20008081 != '\0') {
      g_va_volts = g_reading;
    }
  }
  else if ((g_tdm_channel == 1) && (Mode_DCI_Measure(), DAT_20008081 != '\0')) {
    g_va_amps = g_reading;
  }
  if (g_display_update == '\0') {
    DAT_2000805e = 0;
  }
  else {
    if (g_tdm_channel == 0) {
      bVar1 = 3;
    }
    else {
      bVar1 = 6;
    }
    DAT_200080c5 = DAT_200080c5 + 1;
    if (bVar1 < DAT_200080c5) {
      DAT_200080c5 = 0;
      if (((g_cal_mode != '\0') && (g_tdm_channel != 0)) &&
         (DAT_2000808c != '\0' || DAT_2000808e != '\0')) {
        if (DAT_20008095 < 3) {
          bVar2 = CARRY4(DAT_20007c10,g_va_volts);
          DAT_20007c10 = DAT_20007c10 + g_va_volts;
          DAT_20007c14 = ((int)g_va_volts >> 0x1f) + DAT_20007c14 + (uint)bVar2;
          bVar2 = CARRY4(DAT_20007c18,g_va_amps);
          DAT_20007c18 = DAT_20007c18 + g_va_amps;
          DAT_20007c1c = ((int)g_va_amps >> 0x1f) + DAT_20007c1c + (uint)bVar2;
        }
        else {
          DAT_20007c10 = g_va_volts;
          DAT_20007c18 = g_va_amps;
          DAT_20007c14 = (int)g_va_volts >> 0x1f;
          DAT_20007c1c = (int)g_va_amps >> 0x1f;
        }
        DAT_20008095 = DAT_20008095 - 1;
        if (DAT_20008095 == 0) {
          if (DAT_2000808c == '\0') {
            if (DAT_2000808e != '\0') {
              DAT_2000808e = '\0';
              DAT_2000808f = 1;
              __aeabi_ldivmod(DAT_20007c10,DAT_20007c14,3,0);
              DAT_20007e20 = __aeabi_l2f();
              Cal_ComputeGain(0);
              DAT_20007e2c = g_cal_gain;
              __aeabi_ldivmod(DAT_20007c18,DAT_20007c1c,3,0);
              DAT_20007e20 = __aeabi_l2f();
              Cal_ComputeGain(1);
              DAT_20007e30 = g_cal_gain;
              Cal_StoreField(1,0);
            }
          }
          else {
            DAT_2000808c = '\0';
            DAT_2000808d = 1;
            DAT_20007e24 = __aeabi_ldivmod(DAT_20007c10,DAT_20007c14,3,0);
            DAT_20007e28 = __aeabi_ldivmod(DAT_20007c18,DAT_20007c1c,3,0);
            Cal_StoreField(0,0);
          }
        }
      }
      if (g_tdm_channel == 0) {
        VA_VoltageCorrect();
        VA_VoltsAutorange(g_va_volts);
      }
      else {
        VA_AmpsAutorange(g_va_amps);
      }
      if ((DAT_200080ca != '\0') || (DAT_200080cb != '\0')) {
        DAT_20008046 = 1;
      }
      if (DAT_200080c3 == '\0') {
        g_tdm_channel = g_tdm_channel ^ 1;
        Mode_ConfigureHyCalCap(0);
        VA_ComputePower(g_va_volts,g_va_amps);
        DAT_200080bf = 0;
      }
      g_display_update = '\x01';
      DAT_20008060 = 0;
      DAT_20008062 = 0;
      DAT_2000805e = 1;
      DAT_2000804c = 1;
      if ((g_mode == '\x0f') && (DAT_200080c7 != '\0')) {
        g_reading_fast = g_reading;
      }
      else {
        g_reading_fast = g_reading;
      }
    }
    else {
      g_display_update = '\0';
      DAT_2000805e = 0;
    }
  }
  return;
}


// ==== DCVA_CurrentSignTrim @ 080087f8

/* (volts, amps) -> counts added to the DC VA current (from 08008138, modes 22/23/24): only not cal
   mode, g_pf8_strap_low == 0 and g_variant_ohm6 == 1; |amps| <= 2500 -> 0; else a constant per
   mode, range and sign quadrant of V and I (e.g. mode 22 range 0: +200 / -200 / -300; mode 22
   ranges 0-1 +-2100). medium */

undefined4 DCVA_CurrentSignTrim(int param_1,int param_2)

{
  undefined4 uVar1;
  
  uVar1 = 0;
  if ((g_cal_mode == '\0') &&
     ((((g_mode == 0x16 || (g_mode == 0x17)) || (g_mode == 0x18)) &&
      (g_pf8_strap_low == '\0' && g_variant_ohm6 == '\x01')))) {
    if ((param_2 < 0x9c5) && (-0x9c5 < param_2)) {
      return 0;
    }
    if (g_mode == 0x16) {
      if (g_range == 0) {
        if ((-1 < param_1) && (-1 < param_2)) {
          uVar1 = 200;
        }
        if ((param_1 < 0) && (-1 < param_2)) {
          uVar1 = 200;
        }
        if ((-1 < param_1) && (param_2 < 0)) {
          uVar1 = 0xffffff38;
        }
        if ((param_1 < 0) && (param_2 < 0)) {
          uVar1 = 0xfffffed4;
        }
      }
      else if (g_range == 2) {
        if ((-1 < param_1) && (-1 < param_2)) {
          uVar1 = 0x82;
        }
        if ((param_1 < 0) && (-1 < param_2)) {
          uVar1 = 0x82;
        }
        if ((-1 < param_1) && (param_2 < 0)) {
          uVar1 = 0xffffff56;
        }
        if ((param_1 < 0) && (param_2 < 0)) {
          uVar1 = 0xffffff56;
        }
      }
      else if (g_range < 2) {
        if ((-1 < param_1) && (-1 < param_2)) {
          uVar1 = 0x834;
        }
        if ((param_1 < 0) && (-1 < param_2)) {
          uVar1 = 0x834;
        }
        if ((-1 < param_1) && (param_2 < 0)) {
          uVar1 = 0xfffff7cc;
        }
        if ((param_1 < 0) && (param_2 < 0)) {
          uVar1 = 0xfffff7cc;
        }
      }
      else if (g_range == 3) {
        if ((-1 < param_1) && (-1 < param_2)) {
          uVar1 = 0x83e;
        }
        if ((param_1 < 0) && (-1 < param_2)) {
          uVar1 = 0x83e;
        }
        if ((-1 < param_1) && (param_2 < 0)) {
          uVar1 = 0xfffff7ae;
        }
        if ((param_1 < 0) && (param_2 < 0)) {
          uVar1 = 0xfffff7a4;
        }
      }
    }
    else if (0x15 < g_mode) {
      if (g_mode == 0x18) {
        if (g_range == 0) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x80;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x83;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xffffffbf;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xffffffbf;
          }
        }
        else if (g_range == 2) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 5;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0xc;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xfffffff0;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xffffffee;
          }
        }
        else if (g_range < 2) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x617;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x62d;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xfffffa13;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xfffffa25;
          }
        }
        else if (g_range == 3) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x9c;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0xa0;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xffffff5f;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xffffff61;
          }
        }
      }
      else if (g_mode < 0x18) {
        if (g_range == 0) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x82;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x84;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xffffffb9;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xffffffba;
          }
        }
        else if (g_range == 2) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x52;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x53;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xffffff88;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xffffff88;
          }
        }
        else if (g_range < 2) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x62f;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x645;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xfffff9f4;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xfffffa07;
          }
        }
        else if (g_range == 3) {
          if ((-1 < param_1) && (-1 < param_2)) {
            uVar1 = 0x601;
          }
          if ((param_1 < 0) && (-1 < param_2)) {
            uVar1 = 0x61e;
          }
          if ((-1 < param_1) && (param_2 < 0)) {
            uVar1 = 0xfffff9ba;
          }
          if ((param_1 < 0) && (param_2 < 0)) {
            uVar1 = 0xfffff9d6;
          }
        }
      }
    }
  }
  return uVar1;
}


// ==== Mode_ACVA_OnAD1 @ 08008b8c

/* time-shared V / I (3 / 6 displays). high */

void Mode_ACVA_OnAD1(void)

{
  byte bVar1;
  bool bVar2;
  
  DAT_200080c3 = '\0';
  if (g_tdm_channel == 0) {
    Mode_ACV_OnAD1();
    if (DAT_20008081 != '\0') {
      g_va_volts = g_reading;
    }
  }
  else if ((g_tdm_channel == 1) && (Mode_ACI_Measure(), DAT_20008081 != '\0')) {
    g_va_amps = g_reading;
  }
  if (g_display_update == '\0') {
    DAT_2000805e = 0;
  }
  else {
    if (g_tdm_channel == 0) {
      bVar1 = 3;
    }
    else {
      bVar1 = 6;
    }
    DAT_200080c5 = DAT_200080c5 + 1;
    if (bVar1 < DAT_200080c5) {
      DAT_200080c5 = 0;
      if (((g_cal_mode != '\0') && (g_tdm_channel != 0)) &&
         (DAT_2000808c != '\0' || DAT_2000808e != '\0')) {
        if (DAT_20008095 < 3) {
          bVar2 = CARRY4(DAT_20007c20,g_va_volts);
          DAT_20007c20 = DAT_20007c20 + g_va_volts;
          DAT_20007c24 = ((int)g_va_volts >> 0x1f) + DAT_20007c24 + (uint)bVar2;
          bVar2 = CARRY4(DAT_20007c28,g_va_amps);
          DAT_20007c28 = DAT_20007c28 + g_va_amps;
          DAT_20007c2c = ((int)g_va_amps >> 0x1f) + DAT_20007c2c + (uint)bVar2;
        }
        else {
          DAT_20007c20 = g_va_volts;
          DAT_20007c28 = g_va_amps;
          DAT_20007c24 = (int)g_va_volts >> 0x1f;
          DAT_20007c2c = (int)g_va_amps >> 0x1f;
        }
        DAT_20008095 = DAT_20008095 - 1;
        if (DAT_20008095 == 0) {
          if (DAT_2000808c == '\0') {
            if (DAT_2000808e != '\0') {
              DAT_2000808e = '\0';
              DAT_2000808f = 1;
              __aeabi_ldivmod(DAT_20007c20,DAT_20007c24,3,0);
              DAT_20007e20 = __aeabi_l2f();
              Cal_ComputeGain(0);
              DAT_20007e2c = g_cal_gain;
              __aeabi_ldivmod(DAT_20007c28,DAT_20007c2c,3,0);
              DAT_20007e20 = __aeabi_l2f();
              Cal_ComputeGain(1);
              DAT_20007e30 = g_cal_gain;
              Cal_StoreField(1,0);
            }
          }
          else {
            DAT_2000808c = '\0';
            DAT_2000808d = 1;
            DAT_20007e24 = __aeabi_ldivmod(DAT_20007c20,DAT_20007c24,3,0);
            DAT_20007e28 = __aeabi_ldivmod(DAT_20007c28,DAT_20007c2c,3,0);
            Cal_StoreField(0,0);
          }
        }
      }
      if (g_tdm_channel == 0) {
        VA_VoltageCorrect();
        if ((int)g_va_volts < 0) {
          g_va_volts = -g_va_volts;
        }
        VA_VoltsAutorange(g_va_volts);
      }
      else {
        if ((int)g_va_amps < 0) {
          g_va_amps = -g_va_amps;
        }
        VA_AmpsAutorange(g_va_amps);
      }
      if ((DAT_200080ca != '\0') || (DAT_200080cb != '\0')) {
        DAT_20008046 = 1;
      }
      if (DAT_200080c3 == '\0') {
        g_tdm_channel = g_tdm_channel ^ 1;
        Mode_ConfigureHyCalCap(0);
        VA_ComputePower(g_va_volts,g_va_amps);
        DAT_200080bf = 0;
      }
      g_display_update = '\x01';
      DAT_20008060 = 0;
      DAT_20008062 = 0;
      DAT_2000805e = 1;
      DAT_2000804c = 1;
      if ((g_mode == '\x0f') && (DAT_200080c7 != '\0')) {
        g_reading_fast = g_reading;
      }
      else {
        g_reading_fast = g_reading;
      }
    }
    else {
      g_display_update = '\0';
      DAT_2000805e = 0;
    }
  }
  return;
}


// ==== VA_VoltageCorrect @ 08008ed0

/* VA modes 13/14/15/24 (22/23 skipped; also skipped in cal mode, on current overrange 0x200080cb,
   or V > 60000): bilinear correction added to g_va_volts from the current bin (thresholds
   0x0802896c per range group) and the voltage bin (0x08029094:
   0,10,1000,5000,10000,30000,50000,60000), tables chosen by VA_SelectCorrTable; mode 24 range 2
   allows negative V/I (sign-mixed flag); wrong jack for modes 13/22 (0x200080cc != 1, mA) or 15/24
   (!= 2, A) forces current bin 8. Likely shunt/burden drop compensation. medium */

void VA_VoltageCorrect(void)

{
  bool bVar1;
  undefined1 uVar2;
  byte bVar3;
  byte bVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  
  bVar1 = false;
  uVar2 = 0;
  iVar5 = 1;
  if ((((g_cal_mode == '\0') && (g_mode != '\x16')) && (g_mode != '\x17')) &&
     ((DAT_200080cb == '\0' && (g_va_volts < 0xea61)))) {
    iVar6 = g_va_amps;
    iVar7 = g_va_volts;
    if ((g_mode == '\x18') && (g_range == 2)) {
      if ((g_va_amps < 0) && (-1 < g_va_volts)) {
        uVar2 = 1;
      }
      if ((g_va_volts < 0) && (-1 < g_va_amps)) {
        uVar2 = 1;
      }
      if (g_va_amps < 0) {
        iVar6 = -g_va_amps;
      }
      if (g_va_volts < 0) {
        iVar7 = -g_va_volts;
      }
    }
    else {
      if (g_va_amps < 0) {
        return;
      }
      if (g_va_volts < 0) {
        return;
      }
    }
    if (((g_mode == '\r') || (g_mode == '\x16')) && (g_range < 2)) {
      iVar5 = 0;
    }
    else if (((g_mode == '\x0f') || (g_mode == '\x18')) && (1 < g_range)) {
      iVar5 = 2;
    }
    if ((((g_mode == '\r') || (g_mode == '\x16')) && (DAT_200080cc != '\x01')) ||
       (((g_mode == '\x0f' || (g_mode == '\x18')) && (DAT_200080cc != '\x02')))) {
      bVar1 = true;
      bVar3 = 8;
    }
    else if ((int)(&DAT_0802896c)[iVar5 * 8] < iVar6) {
      if ((int)(&DAT_08028970)[iVar5 * 8] < iVar6) {
        if ((int)(&DAT_08028974)[iVar5 * 8] < iVar6) {
          if ((int)(&DAT_08028978)[iVar5 * 8] < iVar6) {
            if ((int)(&DAT_0802897c)[iVar5 * 8] < iVar6) {
              if ((int)(&DAT_08028980)[iVar5 * 8] < iVar6) {
                bVar3 = 7;
              }
              else {
                bVar3 = 6;
              }
            }
            else {
              bVar3 = 5;
            }
          }
          else {
            bVar3 = 4;
          }
        }
        else {
          bVar3 = 3;
        }
      }
      else {
        bVar3 = 2;
      }
    }
    else {
      bVar3 = 1;
    }
    DAT_20007cf8 = iVar6 - *(int *)(&UNK_08028964 + iVar5 * 0x20 + (uint)bVar3 * 4);
    DAT_20007cfc = (&DAT_08028968)[iVar5 * 8 + (uint)bVar3] -
                   *(int *)(&UNK_08028964 + iVar5 * 0x20 + (uint)bVar3 * 4);
    DAT_20007d00 = (DAT_20007cf8 * 100) / DAT_20007cfc;
    if (100 < DAT_20007d00) {
      DAT_20007d00 = 100;
    }
    if (DAT_20007d00 < -100) {
      DAT_20007d00 = -100;
    }
    if ((iVar7 < 0xb) && (g_mode != '\x0f')) {
      bVar4 = 1;
    }
    else if (iVar7 < 0x3e9) {
      bVar4 = 2;
    }
    else if (iVar7 < 0x1389) {
      bVar4 = 3;
    }
    else if (iVar7 < 0x2711) {
      bVar4 = 4;
    }
    else if (iVar7 < 0x7531) {
      bVar4 = 5;
    }
    else if (iVar7 < 0xc351) {
      bVar4 = 6;
    }
    else {
      bVar4 = 7;
    }
    DAT_20007cdc = iVar7 - *(int *)(&UNK_08029094 + (uint)bVar4 * 4);
    DAT_20007ce0 = (&DAT_08029098)[bVar4] - *(int *)(&UNK_08029094 + (uint)bVar4 * 4);
    DAT_20007ce4 = (DAT_20007cdc * 100) / DAT_20007ce0;
    if (100 < DAT_20007ce4) {
      DAT_20007ce4 = 100;
    }
    if (DAT_20007ce4 < -100) {
      DAT_20007ce4 = -100;
    }
    VA_SelectCorrTable(bVar3,bVar4,uVar2);
    DAT_20007ce8 = DAT_20007ebc - DAT_20007eb8;
    DAT_20007cf0 = (DAT_20007ce4 * DAT_20007ce8) / 100 + DAT_20007eb8;
    DAT_20007cec = DAT_20007ec4 - DAT_20007ec0;
    DAT_20007cf4 = (DAT_20007ce4 * DAT_20007cec) / 100 + DAT_20007ec0;
    if (bVar1) {
      DAT_20007d04 = DAT_20007cf4;
    }
    else {
      DAT_20007d04 = (DAT_20007d00 * (DAT_20007cf4 - DAT_20007cf0)) / 100 + DAT_20007cf0;
    }
    if (((g_pf8_strap_low == '\0') || (g_mode != '\x18')) || (g_range != 2)) {
      g_va_volts = DAT_20007d04 + g_va_volts;
    }
    else if (g_va_volts < 0) {
      g_va_volts = g_va_volts - DAT_20007d04;
    }
    else {
      g_va_volts = DAT_20007d04 + g_va_volts;
    }
  }
  return;
}


// ==== VA_SelectCorrTable @ 08009364

/* (current bin, voltage bin, sign-mixed) -> 0x20007eb8/ebc (row) and 0x20007ec0/ec4 (next row) from
   per-mode/range/PF8-strap tables 0x080260c0..0x080284c0 (8-word rows); modes 22/23 -> zeros. Mode
   15 ranges 2/3 (strap high) use 7-word rows (0x080283a4, 0x080284a0), so voltage bin 7 reads the
   next row's first entry: possible table-layout bug, unverified. high */

void VA_SelectCorrTable(byte param_1,byte param_2,byte param_3)

{
  if (g_mode == '\r') {
    if (g_range == 0) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = (&PTR_RCC_CFGR_080260c0)[(uint)param_2 + (uint)param_1 * 8];
        DAT_20007ebc = (&DAT_080260c4)[(uint)param_2 + (uint)param_1 * 8];
        DAT_20007ec0 = (&PTR_PWR_CSR_080260e0)[(uint)param_2 + (uint)param_1 * 8];
        DAT_20007ec4 = *(undefined4 *)(&UNK_080260e4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_080261e0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080261e4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026200 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026204 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08026540 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026544 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026560 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026564 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08026660 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026664 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026680 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026684 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range < 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08026300 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026304 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026320 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026324 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08026420 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026424 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026440 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026444 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 3) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08026780 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026784 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080267a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080267a4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_080268a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080268a4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080268c0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080268c4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
  }
  else if (g_mode == '\x0e') {
    if (g_range == 0) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_080269c0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080269c4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080269e0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080269e4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08026ae0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026ae4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026b00 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026b04 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08026e40 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026e44 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026e60 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026e64 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08026f60 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026f64 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026f80 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026f84 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range < 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08026c00 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026c04 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026c20 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026c24 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08026d20 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08026d24 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08026d40 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08026d44 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 3) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08027080 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027084 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080270a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080270a4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_080271a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080271a4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080271c0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080271c4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
  }
  else if (g_mode == '\x0f') {
    if (g_range == 0) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_080272c0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080272c4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080272e0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080272e4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_080273e0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080273e4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027400 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027404 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_080283a4 + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080283a8 + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080283c0 + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080283c4 + (uint)param_2 * 4 + (uint)param_1 * 0x1c);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08027740 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027744 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027760 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027764 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range < 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08027500 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027504 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027520 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027524 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08027620 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027624 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027640 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027644 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 3) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_080284a0 + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_080284a4 + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080284bc + (uint)param_1 * 0x1c + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080284c0 + (uint)param_2 * 4 + (uint)param_1 * 0x1c);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08027860 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027864 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027880 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027884 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
  }
  else if ((g_mode == '\x16') || (g_mode == '\x17')) {
    DAT_20007eb8 = (undefined *)0x0;
    DAT_20007ebc = 0;
    DAT_20007ec0 = (undefined *)0x0;
    DAT_20007ec4 = 0;
  }
  else if (g_mode == '\x18') {
    if (g_range == 0) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08027980 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027984 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080279a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080279a4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08027aa0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027aa4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027ac0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027ac4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 2) {
      if ((g_pf8_strap_low & param_3) == 0) {
        if (g_pf8_strap_low == 0) {
          DAT_20007eb8 = *(undefined **)(&UNK_08027e00 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ebc = *(undefined4 *)(&UNK_08027e04 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ec0 = *(undefined **)(&UNK_08027e20 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ec4 = *(undefined4 *)(&UNK_08027e24 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
        }
        else {
          DAT_20007eb8 = *(undefined **)(&UNK_08027f20 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ebc = *(undefined4 *)(&UNK_08027f24 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ec0 = *(undefined **)(&UNK_08027f40 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
          DAT_20007ec4 = *(undefined4 *)(&UNK_08027f44 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
        }
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08028040 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08028044 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08028060 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08028064 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range < 2) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08027bc0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027bc4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027be0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027be4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08027ce0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08027ce4 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08027d00 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08027d04 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
    else if (g_range == 3) {
      if (g_pf8_strap_low == 0) {
        DAT_20007eb8 = *(undefined **)(&UNK_08028160 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08028164 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_08028180 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_08028184 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
      else {
        DAT_20007eb8 = *(undefined **)(&UNK_08028280 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ebc = *(undefined4 *)(&UNK_08028284 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec0 = *(undefined **)(&UNK_080282a0 + (uint)param_1 * 0x20 + (uint)param_2 * 4);
        DAT_20007ec4 = *(undefined4 *)(&UNK_080282a4 + (uint)param_2 * 4 + (uint)param_1 * 0x20);
      }
    }
  }
  return;
}


// ==== VA_VoltsAutorange @ 0800a260

/* VA voltage channel: |V| >= 55,001 -> overrange flag 0x200080ca, and if not up yet set 0x200080c6
   (+0x200080c8) and re-range; < 3200 while up -> down; g_range = 080148f6 (bit 0 = V high
   0x200080c6, bit 1 = I high 0x200080c7), then 0800d8c2 reconfigures; not in cal mode or with
   0x20008042/0x2000804e set. Stores V in 0x20007eac; SETUP page 10 flags 0x20008062. high */

undefined4 VA_VoltsAutorange(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  
  DAT_200080ca = 0;
  DAT_200080c0 = 0;
  iVar1 = param_1;
  if ((((g_mode == '\x16') || (g_mode == '\x17')) || (g_mode == '\x18')) && (param_1 < 0)) {
    iVar1 = -param_1;
  }
  if (iVar1 < 0xd6d9) {
    if (((iVar1 < 0xc80) && (g_cal_mode == '\0' && DAT_20008042 == '\0')) &&
       ((DAT_2000804e == '\0' && (DAT_200080c6 != '\0')))) {
      DAT_200080c6 = '\0';
      g_range = VA_RangeFromFlags();
      VA_ApplyRange();
    }
  }
  else {
    DAT_200080ca = 1;
    if (((g_cal_mode == '\0' && DAT_20008042 == '\0') && (DAT_2000804e == '\0')) &&
       (DAT_200080c6 == '\0')) {
      DAT_200080c6 = '\x01';
      DAT_200080c8 = 1;
      g_range = VA_RangeFromFlags();
      VA_ApplyRange();
    }
  }
  DAT_20007eac = param_1;
  if (g_setup_page == '\n') {
    DAT_20008062 = 1;
  }
  return param_4;
}


// ==== VA_AmpsAutorange @ 0800a358

/* VA current channel: up above 55,000 (modes 15/24 already high: 11,000), down below 4000 (400);
   overrange flag 0x200080cb; same g_range recompute. Stores I in 0x20007eb0 (modes 13/22: /100 low,
   /10 high; raw kept in 0x20007eb4); SETUP page 11 flags 0x20008062. high */

undefined4 VA_AmpsAutorange(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  if (((g_mode == '\x0f') || (g_mode == '\x18')) && (DAT_200080c7 != '\0')) {
    iVar1 = 11000;
    iVar2 = 400;
  }
  else {
    iVar1 = 55000;
    iVar2 = 4000;
  }
  iVar3 = param_1;
  if ((((g_mode == '\x16') || (g_mode == '\x17')) || (g_mode == '\x18')) && (param_1 < 0)) {
    iVar3 = -param_1;
  }
  DAT_200080cb = 0;
  DAT_200080c1 = 0;
  if (iVar1 < iVar3) {
    DAT_200080cb = 1;
    if (((g_cal_mode == '\0' && DAT_20008042 == '\0') && (DAT_2000804e == '\0')) &&
       (DAT_200080c7 == '\0')) {
      DAT_200080c7 = '\x01';
      g_range = VA_RangeFromFlags();
      VA_ApplyRange();
    }
  }
  else if (((iVar3 < iVar2) && (g_cal_mode == '\0' && DAT_20008042 == '\0')) &&
          ((DAT_2000804e == '\0' && (DAT_200080c7 != '\0')))) {
    DAT_200080c7 = '\0';
    g_range = VA_RangeFromFlags();
    VA_ApplyRange();
  }
  if ((g_mode == '\r') || (g_mode == '\x16')) {
    if (DAT_200080c7 == '\0') {
      DAT_20007eb0 = param_1 / 100;
    }
    else if (DAT_200080c7 == '\x01') {
      DAT_20007eb0 = param_1 / 10;
    }
    DAT_20007eb4 = param_1;
  }
  else {
    DAT_20007eb0 = param_1;
  }
  if (g_setup_page == '\v') {
    DAT_20008062 = 1;
  }
  return param_4;
}


// ==== VA_ComputePower @ 0800a4ac

/* (volts, amps) -> g_reading = |V| * |I| / 100000 (uA and mA VA, and A VA range < 2) or / 10000 (A
   VA range >= 2), 64-bit divide; sign of the power is dropped. From Mode_DCVA_OnAD1 and
   Mode_ACVA_OnAD1. high (emulated) */

void VA_ComputePower(uint param_1,uint param_2)

{
  undefined4 uVar1;
  
  if ((int)param_1 < 0) {
    param_1 = -param_1;
  }
  if ((int)param_2 < 0) {
    param_2 = -param_2;
  }
  if ((g_mode == '\x0e') || (g_mode == '\x17')) {
    uVar1 = 100000;
  }
  else if ((g_mode == '\r') || (g_mode == '\x16')) {
    uVar1 = 100000;
  }
  else if (g_range < 2) {
    uVar1 = 100000;
  }
  else {
    uVar1 = 10000;
  }
  g_reading = __aeabi_ldivmod((int)((ulonglong)param_1 * (ulonglong)param_2),
                              ((int)param_1 >> 0x1f) * param_2 +
                              param_1 * ((int)param_2 >> 0x1f) +
                              (int)((ulonglong)param_1 * (ulonglong)param_2 >> 0x20),uVar1,0);
  return;
}


// ==== VA_Reset @ 0800a560

/* VA restart (Mode_ApplyChange, VA_ApplyRange): zero VA volts 0x20007eac / amps 0x20007eb0 and
   overrange flags 0x200080ca/cb, set the "-----" flags 0x200080bf..c1, display + LCD refresh,
   clears LCD shadow icon bits (08017fb4(0)). high */

undefined4 VA_Reset(void)

{
  undefined4 unaff_r7;
  
  DAT_20007eac = 0;
  DAT_20007eb0 = 0;
  DAT_200080ca = 0;
  DAT_200080cb = 0;
  DAT_200080bf = 1;
  DAT_200080c0 = 1;
  DAT_200080c1 = 1;
  g_display_update = 1;
  DAT_20008062 = 1;
  LCD_SetSubDP(0);
  return unaff_r7;
}


// ==== HY_RMS_DiscardLowZ @ 0800a5b2

/* HY_OnRMS target for LowZ (mode 0): thunk to HY_RMS_Discard. high */

undefined4 HY_RMS_DiscardLowZ(void)

{
  undefined4 unaff_r7;
  
  HY_RMS_Discard();
  return unaff_r7;
}


// ==== HY_RMS_Discard @ 0800a5ba

/* empty (2 bytes): the HY3131 digital RMS result is not used. high */

void HY_RMS_Discard(void)

{
  return;
}


// ==== ACDC_Step @ 0800a5bc

/* (dc phase) AC+DC mode sample counter 0x200080be: AC phase (0, from ACV_AfterReading) ends after 5
   readings -> ACDC_PhaseEnd(1); DC phase (1, from Mode_DCV_OnAD1) after 3 -> ACDC_PhaseEnd(0).
   medium */

void ACDC_Step(char param_1)

{
  if (param_1 == '\0') {
    DAT_200080be = DAT_200080be + 1;
    if (4 < DAT_200080be) {
      ACDC_PhaseEnd(1);
    }
  }
  else {
    DAT_200080be = DAT_200080be + 1;
    if (2 < DAT_200080be) {
      ACDC_PhaseEnd(0);
    }
  }
  return;
}


// ==== ACDC_PhaseEnd @ 0800a608

/* (1 = AC done, 0 = DC done): saves g_reading as AC 0x20007e9c (flag 0x200080bc) or DC 0x20007ea0
   (0x200080bb), phase 0x200080bd = arg; on DC done ACDC_Combine, Reading_StabilityFilter(20,10),
   display update, fast = slow; then HY_SetMode/Cal_LoadRecord to switch the HY config. medium */

void ACDC_PhaseEnd(char param_1)

{
  DAT_200080be = 0;
  if (param_1 == '\x01') {
    DAT_20007e9c = g_reading;
    DAT_200080bc = 1;
  }
  else {
    DAT_20007ea0 = g_reading;
    DAT_200080bb = 1;
  }
  DAT_200080bd = param_1;
  if (param_1 == '\0') {
    ACDC_Combine();
    g_reading = Reading_StabilityFilter(g_reading,0x14,10);
    g_display_update = 1;
    DAT_2000805e = 1;
    g_reading_fast = g_reading;
  }
  HY_SetMode(g_mode,g_range);
  Cal_LoadRecord(g_mode,g_range);
  return;
}


// ==== ACDC_Combine @ 0800a6a8

/* AC+DC total: g_reading = (int)sqrt(AC^2 + DC^2) from |0x20007e9c|, |0x20007ea0| (squares kept in
   0x20007ea4/a8); either > 60,000 -> 100000 (OL). high (emulated) */

undefined4 ACDC_Combine(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20007e9c < 0) {
    DAT_20007e9c = -DAT_20007e9c;
  }
  if (DAT_20007ea0 < 0) {
    DAT_20007ea0 = -DAT_20007ea0;
  }
  if ((DAT_20007e9c < 0xea61) && (DAT_20007ea0 < 0xea61)) {
    DAT_20007ea4 = DAT_20007e9c * DAT_20007e9c;
    DAT_20007ea8 = DAT_20007ea0 * DAT_20007ea0;
    __aeabi_ui2d(DAT_20007ea8 + DAT_20007ea4);
    sqrt();
    g_reading = __aeabi_d2iz();
  }
  else {
    g_reading = 100000;
  }
  return unaff_r7;
}


// ==== ACV_ComputeDbm @ 0800a74c  [DAT]

/* 0x20007e94 (float) = 10*log10(V^2/0.6), dBm into 600 ohm; V = g_reading / 10000, 1000, 100, 10
   for ranges 0..3 (range > 3 uses an uninitialized register). Shown by LCD_SubDbm. high (emulated)
    */

undefined4 ACV_ComputeDbm(void)

{
  undefined4 uVar1;
  undefined4 in_r3;
  undefined4 unaff_r4;
  undefined8 uVar2;
  
  if (g_range == 0) {
    uVar1 = __aeabi_i2f(g_reading);
    unaff_r4 = __aeabi_fdiv(uVar1,0x461c4000);
  }
  else if (g_range == 2) {
    uVar1 = __aeabi_i2f(g_reading);
    unaff_r4 = __aeabi_fdiv(uVar1,&DAT_42c80000);
  }
  else if (g_range < 2) {
    uVar1 = __aeabi_i2f(g_reading);
    unaff_r4 = __aeabi_fdiv(uVar1,0x447a0000);
  }
  else if (g_range == 3) {
    uVar1 = __aeabi_i2f(g_reading);
    unaff_r4 = __aeabi_fdiv(uVar1,0x41200000);
  }
  __aeabi_fmul(unaff_r4,unaff_r4);
  uVar2 = __aeabi_f2d();
  __aeabi_ddiv((int)uVar2,(int)((ulonglong)uVar2 >> 0x20),0x33333333,0x3fe33333);
  uVar1 = __aeabi_d2f();
  __aeabi_f2d(uVar1);
  uVar2 = log10();
  __aeabi_dmul((int)uVar2,(int)((ulonglong)uVar2 >> 0x20),0,0x40240000);
  DAT_20007e94 = __aeabi_d2f();
  return in_r3;
}


// ==== HY_RMS_NopACmV @ 0800a7f8

/* HY_OnRMS target for ACmV (mode 4): empty, the HY3131 RMS result is ignored. high */

void HY_RMS_NopACmV(void)

{
  return;
}


// ==== HY_RMS_NopACI @ 0800a7fa

/* HY_OnRMS target for AC current modes 16/18/20: empty. high */

void HY_RMS_NopACI(void)

{
  return;
}


// ==== HY_OnPeak @ 0800a7fc

/* peak-hold ready (R29 bit 2), not in AC+DC. medium */

undefined1 HY_OnPeak(void)

{
  undefined1 unaff_r7;
  undefined1 auStack_88 [128];
  
  if (((g_mode != '\r') && (g_mode != '\x0e')) && (g_mode != '\x0f')) {
    DAT_20007df0 = (uint)DAT_200077e1 + (uint)DAT_200077e2 * 0x100 + (uint)DAT_200077e3 * 0x10000;
    DAT_20007dec = (uint)DAT_200077de + (uint)DAT_200077df * 0x100 + (uint)DAT_200077e0 * 0x10000;
    HY_Write(auStack_88 + 0x80,0x29,1);
    Delay_Loop(10);
    unaff_r7 = DAT_2000807e;
    HY_Write(auStack_88 + 0x80,0x29,1);
    if (((g_mode == '\x02') || (g_mode == '\x04')) && (DAT_200080b7 == '\x01')) {
      if (DAT_200080b8 == '\0') {
        if (DAT_20007df0 < 0x40000) {
          g_reading = DAT_20007df0;
          if (DAT_2000808c == '\0') {
            if (DAT_2000808e == '\0') {
              if (DAT_20000238 == '\0') {
                g_reading = Cal_SubtractOffsetAC(DAT_20007df0);
                if ((int)g_reading < 0) {
                  g_reading = 0;
                }
                g_reading = Cal_DivideGain(g_reading);
                if (g_cal_mode == '\0') {
                  if (DAT_20007e8c < g_reading) {
                    DAT_20007e8c = g_reading;
                  }
                  else {
                    g_reading = DAT_20007e8c;
                  }
                }
              }
              DAT_20008045 = 0;
            }
            else {
              Cal_AcGainCapture(DAT_20007df0,DAT_20007e08);
            }
          }
          else {
            DAT_20007e08 = Cal_AcOffsetCapture(DAT_20007df0);
          }
          g_display_update = 1;
          DAT_2000805e = 1;
          g_reading_fast = g_reading;
        }
      }
      else {
        g_reading = ~DAT_20007dec & 0x3ffff;
        if (DAT_20000238 == '\0') {
          if (DAT_20007e90 < g_reading) {
            DAT_20007df4 = DAT_20007dec;
            DAT_20007e90 = g_reading;
          }
          else {
            g_reading = DAT_20007e90;
          }
        }
        DAT_20008045 = 1;
      }
    }
  }
  return unaff_r7;
}


// ==== HY_OnCounter @ 0800aa00

/* counter done (CTF). medium */

undefined4 HY_OnCounter(void)

{
  undefined4 unaff_r7;
  
  if (g_mode == '\r') {
    return unaff_r7;
  }
  if (g_mode == '\x0e') {
    return unaff_r7;
  }
  if (g_mode == '\x0f') {
    return unaff_r7;
  }
  HY_CounterRead();
  DAT_20007fc2 = 0;
  if (((g_mode != '\0') && (g_mode != '\x02')) && (g_mode != '\x04')) {
    if (g_mode == '\x06') {
      Counter_Frequency(0,g_range);
      goto LAB_0800aa8a;
    }
    if (g_mode == '\a') {
      Counter_PulseWidth();
      goto LAB_0800aa8a;
    }
    if (g_mode == '\b') {
      Counter_Duty();
      goto LAB_0800aa8a;
    }
    if (g_mode == '\f') {
      Mode_Cap_OnCounter();
      goto LAB_0800aa8a;
    }
    if (((g_mode != '\x10') && (g_mode != '\x12')) && (g_mode != '\x14')) goto LAB_0800aa8a;
  }
  Counter_Frequency(1,DAT_2000803e);
LAB_0800aa8a:
  DAT_20007fc0 = 0;
  if (((g_mode == '\x06') || (g_mode == '\a')) || (g_mode == '\b')) {
    DAT_20008060 = 1;
  }
  if ((((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
       ((g_mode == '\x02' && (DAT_200080ba == '\0')))) || (g_mode == '\x04')) && (g_mode != '\0')) {
    LCD_DrawSubUnits();
  }
  if ((((g_mode == '\x10') || (g_mode == '\x12')) ||
      ((g_mode == '\x14' || ((g_mode == '\x02' || (g_mode == '\x04')))))) && (DAT_200080a5 == '\0'))
  {
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== HY_CounterRead @ 0800ab48

/* from HY_OnCounter: three 24-bit counter values from the HY3131 read buffer (0x200077e5..ed) ->
   0x20007e44, 0x20007e40, 0x20007e3c; gate length 0x20007e38 = 0x1000000 - preset 0x2000001c * 256,
   total 0x20007e48 = e38 + e3c; HY_CounterRearm. medium */

undefined4 HY_CounterRead(void)

{
  undefined4 unaff_r7;
  
  DAT_20007e3c = (uint)DAT_200077eb + (uint)DAT_200077ec * 0x100 + (uint)DAT_200077ed * 0x10000;
  DAT_20007e40 = (uint)DAT_200077e8 + (uint)DAT_200077e9 * 0x100 + (uint)DAT_200077ea * 0x10000;
  DAT_20007e44 = (uint)DAT_200077e5 + (uint)DAT_200077e6 * 0x100 + (uint)DAT_200077e7 * 0x10000;
  DAT_20007e38 = DAT_2000001c * -0x100 + 0x1000000;
  DAT_20007e48 = DAT_20007e38 + DAT_20007e3c;
  HY_CounterRearm();
  return unaff_r7;
}


// ==== Counter_Frequency @ 0800ac1c

/* medium */

undefined4 Counter_Frequency(char param_1,byte param_2,undefined4 param_3,undefined4 param_4)

{
  Counter_ComputeFrequency();
  DAT_20008045 = 0;
  DAT_20008046 = 0;
  if ((param_1 == '\0') && (g_cal_mode == '\0' && DAT_20008042 == '\x01')) {
    if (param_2 == 0) {
      if ((DAT_20007c44 == 0) && (DAT_20007c40 < 100000)) {
        Counter_ScaleToRange(0,0);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (param_2 == 2) {
      if ((DAT_20007c44 == 0) && (DAT_20007c40 < 10000000)) {
        Counter_ScaleToRange(0,2);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (param_2 < 2) {
      if ((DAT_20007c44 == 0) && (DAT_20007c40 < 1000000)) {
        Counter_ScaleToRange(0,1);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (param_2 == 4) {
      if ((DAT_20007c44 == 0) && (DAT_20007c40 < 1000000000)) {
        Counter_ScaleToRange(0,4);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (param_2 < 4) {
      if ((DAT_20007c44 == 0) && (DAT_20007c40 < 100000000)) {
        Counter_ScaleToRange(0,3);
      }
      else {
        DAT_20008046 = 1;
      }
    }
  }
  else if ((DAT_20007c44 == 0) && (DAT_20007c40 < 1000000000)) {
    if (DAT_20007c40 < 100000000) {
      if ((param_2 == 4) && (79999999 < DAT_20007c40)) {
        Counter_ScaleToRange(param_1,4);
      }
      else if (DAT_20007c40 < 10000000) {
        if ((param_2 == 3) && (7999999 < DAT_20007c40)) {
          Counter_ScaleToRange(param_1,3);
        }
        else if (DAT_20007c40 < 1000000) {
          if ((param_2 == 2) && (799999 < DAT_20007c40)) {
            Counter_ScaleToRange(param_1,2);
          }
          else if (DAT_20007c40 < 100000) {
            if ((param_2 == 1) && (79999 < DAT_20007c40)) {
              Counter_ScaleToRange(param_1,1);
            }
            else {
              Counter_ScaleToRange(param_1,0);
            }
          }
          else {
            Counter_ScaleToRange(param_1,1);
          }
        }
        else {
          Counter_ScaleToRange(param_1,2);
        }
      }
      else {
        Counter_ScaleToRange(param_1,3);
      }
    }
    else {
      Counter_ScaleToRange(param_1,4);
    }
  }
  else {
    Counter_ScaleToRange(param_1,9);
  }
  if (param_1 == '\0') {
    g_display_update = 1;
    DAT_2000803f = Display_DecimalPlaces(g_mode,g_range);
  }
  return param_4;
}


// ==== Counter_ScaleToRange @ 0800aee8

/* (dest, range) from Counter_Frequency: 64-bit count 0x20007c40 rounded and divided by 10^range
   (adds 5/50/500/5000 into the count itself, __aeabi_uldivmod 08014028). dest 0 -> g_reading +
   g_range; 1 -> secondary frequency 0x20007d84/0x20007e60/0x20007e64 and range
   0x2000803e/0x200080a0/0x200080a1. range 9 = overflow: sets 0x20008046 (main) / 0x20008047
   (secondary), range 4, and stores the caller's leftover r7 as the value (uninitialized). high */

void Counter_ScaleToRange(char param_1,char param_2)

{
  undefined1 unaff_r6;
  undefined4 unaff_r7;
  bool bVar1;
  
  if (param_2 == '\0') {
    unaff_r6 = 0;
    unaff_r7 = __aeabi_uldivmod(DAT_20007c40,DAT_20007c44,1,0);
  }
  else if (param_2 == '\x01') {
    unaff_r6 = 1;
    bVar1 = 0xfffffffa < DAT_20007c40;
    DAT_20007c40 = DAT_20007c40 + 5;
    DAT_20007c44 = DAT_20007c44 + (uint)bVar1;
    unaff_r7 = __aeabi_uldivmod(DAT_20007c40,DAT_20007c44,10,0);
  }
  else if (param_2 == '\x02') {
    unaff_r6 = 2;
    bVar1 = 0xffffffcd < DAT_20007c40;
    DAT_20007c40 = DAT_20007c40 + 0x32;
    DAT_20007c44 = DAT_20007c44 + (uint)bVar1;
    unaff_r7 = __aeabi_uldivmod(DAT_20007c40,DAT_20007c44,100,0);
  }
  else if (param_2 == '\x03') {
    unaff_r6 = 3;
    bVar1 = 0xfffffe0b < DAT_20007c40;
    DAT_20007c40 = DAT_20007c40 + 500;
    DAT_20007c44 = DAT_20007c44 + (uint)bVar1;
    unaff_r7 = __aeabi_uldivmod(DAT_20007c40,DAT_20007c44,1000,0);
  }
  else if (param_2 == '\x04') {
    unaff_r6 = 4;
    bVar1 = 0xffffec77 < DAT_20007c40;
    DAT_20007c40 = DAT_20007c40 + 5000;
    DAT_20007c44 = DAT_20007c44 + (uint)bVar1;
    unaff_r7 = __aeabi_uldivmod(DAT_20007c40,DAT_20007c44,10000,0);
  }
  else if (param_2 == '\t') {
    unaff_r6 = 4;
    if (param_1 == '\x01') {
      DAT_20008047 = 1;
    }
    else {
      DAT_20008046 = 1;
    }
  }
  if (param_1 == '\0') {
    g_reading = unaff_r7;
    g_range = unaff_r6;
  }
  else {
    DAT_20007d84 = unaff_r7;
    DAT_20007e60 = unaff_r7;
    DAT_20007e64 = unaff_r7;
    DAT_2000803e = unaff_r6;
    DAT_200080a0 = unaff_r6;
    DAT_200080a1 = unaff_r6;
  }
  return;
}


// ==== Counter_PulseWidth @ 0800b070

/* medium */

undefined4 Counter_PulseWidth(void)

{
  undefined4 in_r3;
  undefined8 uVar1;
  
  DAT_20008045 = 0;
  Counter_ComputePeriod();
  Counter_ComputeDuty();
  uVar1 = __aeabi_uldivmod((int)((ulonglong)DAT_20007e34 * (ulonglong)DAT_20007c48),
                           DAT_20007e34 * DAT_20007c4c +
                           (int)((ulonglong)DAT_20007e34 * (ulonglong)DAT_20007c48 >> 0x20),1000,0);
  DAT_20007c4c = (int)((ulonglong)uVar1 >> 0x20);
  DAT_20007c48 = (uint)uVar1;
  if (g_cal_mode == '\0' && DAT_20008042 == '\x01') {
    if (g_range == 0) {
      if ((DAT_20007c4c == 0) && (DAT_20007c48 < 100000)) {
        Counter_PeriodToRange(0);
        uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (g_range == 2) {
      if ((DAT_20007c4c == 0) && (DAT_20007c48 < 10000000)) {
        Counter_PeriodToRange(2);
        uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
      }
      else {
        DAT_20008046 = 1;
      }
    }
    else if (g_range < 2) {
      if ((DAT_20007c4c == 0) && (DAT_20007c48 < 1000000)) {
        Counter_PeriodToRange(1);
        uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
      }
      else {
        DAT_20008046 = 1;
      }
    }
  }
  else if ((DAT_20007c4c == 0) && (DAT_20007c48 < 10000000)) {
    if (DAT_20007c48 < 1000000) {
      if ((g_range == 2) && (799999 < DAT_20007c48)) {
        Counter_PeriodToRange(2);
        uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
      }
      else if (DAT_20007c48 < 100000) {
        if ((g_range == 1) && (79999 < DAT_20007c48)) {
          Counter_PeriodToRange(1);
          uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
        }
        else {
          Counter_PeriodToRange(0);
          uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
        }
      }
      else {
        Counter_PeriodToRange(1);
        uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
      }
    }
    else {
      Counter_PeriodToRange(2);
      uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
    }
  }
  else {
    Counter_PeriodToRange(9);
    uVar1 = CONCAT44(DAT_20007c4c,DAT_20007c48);
  }
  DAT_20007c4c = (int)((ulonglong)uVar1 >> 0x20);
  DAT_20007c48 = (uint)uVar1;
  g_display_update = 1;
  DAT_2000803f = Display_DecimalPlaces(g_mode,g_range);
  return in_r3;
}


// ==== Counter_PeriodToRange @ 0800b244

/* (range) from Counter_PulseWidth: g_range = 0/1/2 and g_reading = 64-bit period 0x20007c48 / 1,
   10, 100; 9 -> range 2 with OL flag 0x20008046. medium */

void Counter_PeriodToRange(char param_1)

{
  if (param_1 == '\0') {
    g_range = 0;
    g_reading = __aeabi_uldivmod(DAT_20007c48,DAT_20007c4c,1,0);
  }
  else if (param_1 == '\x01') {
    g_range = 1;
    g_reading = __aeabi_uldivmod(DAT_20007c48,DAT_20007c4c,10,0);
  }
  else if (param_1 == '\x02') {
    g_range = 2;
    g_reading = __aeabi_uldivmod(DAT_20007c48,DAT_20007c4c,100,0);
  }
  else if (param_1 == '\t') {
    g_range = 2;
    DAT_20008046 = 1;
  }
  return;
}


// ==== Counter_Duty @ 0800b2d8

/* medium */

undefined4 Counter_Duty(void)

{
  undefined4 unaff_r7;
  
  DAT_20008045 = 0;
  Counter_ComputeDuty();
  g_reading = DAT_20007e34;
  g_display_update = 1;
  return unaff_r7;
}


// ==== Mode_Cap_OnCounter @ 0800b33c

/* capacitance result on HY_OnCounter (mode 12): 0x20007e44 / 0x20007e40 (x100 on ranges 2-4);
   ranges 0-1: subtract g_cal_offset (floor 0) and divide by g_cal_gain unless 0, cal offset capture
   0800f034 / gain 0800f6c0; ranges 2-5: gain only (capture 0800f470);
   Reading_StabilityFilter(threshold value/50, range 5 value/10, min 20; n 10); ranges 2-4 below 21
   -> 0; range 4 can skip display updates by countdown 0x200080a4; then 0800ec28. high */

void Mode_Cap_OnCounter(void)

{
  ushort uVar1;
  undefined4 uVar2;
  char cVar3;
  
  DAT_20008045 = 0;
  if (g_range == 5) {
    DAT_20007e68 = DAT_20007e44 / DAT_20007e40;
  }
  else if (g_range < 2) {
    DAT_20007e68 = DAT_20007e44 / DAT_20007e40;
  }
  else {
    DAT_20007e68 = (DAT_20007e44 * 100) / DAT_20007e40;
  }
  if (g_range < 2) {
    if (DAT_2000808c == '\0') {
      if (DAT_2000808e == '\0') {
        if (g_cal_offset < DAT_20007e68) {
          DAT_20007e68 = DAT_20007e68 - g_cal_offset;
        }
        else {
          DAT_20007e68 = 0;
        }
        cVar3 = '\x01';
        __aeabi_cfcmpeq(g_cal_gain);
        if ((cVar3 == '\0') && (DAT_20007e68 != 0)) {
          uVar2 = __aeabi_ui2f(DAT_20007e68);
          __aeabi_fdiv(uVar2,g_cal_gain);
          DAT_20007e68 = __aeabi_f2uiz();
        }
      }
      else {
        Cal_CapGainCapture(DAT_20007e68,DAT_20007d08);
      }
    }
    else {
      DAT_20007d08 = Cal_CapOffsetCapture(DAT_20007e68);
    }
  }
  else if (DAT_2000808e == '\0') {
    uVar2 = __aeabi_ui2f(DAT_20007e68);
    __aeabi_fdiv(uVar2,g_cal_gain);
    DAT_20007e68 = __aeabi_f2uiz();
  }
  else {
    Cal_DcGainCapture(DAT_20007e68);
  }
  if (g_range == 5) {
    uVar1 = (ushort)(DAT_20007e68 / 10);
  }
  else {
    uVar1 = (ushort)(DAT_20007e68 / 0x32);
  }
  if (uVar1 < 0x14) {
    uVar1 = 0x14;
  }
  g_reading = Reading_StabilityFilter(DAT_20007e68,uVar1,10);
  if (((1 < g_range) && (g_range < 5)) && (g_reading < 0x15)) {
    g_reading = 0;
  }
  DAT_20007fc8 = DAT_20007fc6;
  if ((((DAT_20008042 & (g_cal_mode ^ 1)) == 0) || (g_range != 4)) || (DAT_200080a4 == '\0')) {
    g_display_update = 1;
  }
  else {
    DAT_200080a4 = DAT_200080a4 + -1;
  }
  Cap_SetTimeout();
  return;
}


// ==== Mode_Cap_Poll @ 0800b540

/* capacitance: range forcing after a measurement. medium */

undefined4 Mode_Cap_Poll(void)

{
  undefined4 unaff_r7;
  
  if (DAT_200080a3 != '\0') {
    DAT_200080a3 = '\0';
    Cap_SetTimeout();
    if (((DAT_20008042 & (g_cal_mode ^ 1)) != 0) && (g_range == 4)) {
      DAT_200080a4 = 2;
    }
    if (DAT_20008042 == 0 && g_cal_mode == 0) {
      if (g_range < 2) {
        g_range = 2;
      }
      else if (g_range < 5) {
        g_range = 5;
      }
      DAT_2000807c = 0;
      DAT_2000804a = 0;
      DAT_2000804b = 1;
      if ((g_range == 2) && (DAT_200080a2 != '\0')) {
        DAT_200080a2 = '\0';
        DAT_20008046 = 0;
        g_range = 0;
      }
    }
    else {
      DAT_20008046 = 1;
      g_display_update = 1;
    }
  }
  return unaff_r7;
}


// ==== Counter_ComputeFrequency @ 0800b610

/* from Counter_Frequency: Hz gain capture (0x2000808e, mode 6) -> Counter_CalibrateClock first;
   then 64-bit 0x20007c40 = clock 0x20007e18 * count 0x20007e40 * 10 / (gate 0x20007e48 / 10).
   medium */

undefined4 Counter_ComputeFrequency(void)

{
  longlong lVar1;
  undefined4 in_r3;
  undefined8 uVar2;
  
  if ((DAT_2000808e != '\0') && (g_mode == '\x06')) {
    Counter_CalibrateClock();
  }
  lVar1 = ((ulonglong)DAT_20007e18 * (ulonglong)DAT_20007e40 & 0xffffffff) * 10;
  uVar2 = __aeabi_uldivmod((int)lVar1,
                           (((int)DAT_20007e18 >> 0x1f) * DAT_20007e40 +
                           (int)((ulonglong)DAT_20007e18 * (ulonglong)DAT_20007e40 >> 0x20)) * 10 +
                           (int)((ulonglong)lVar1 >> 0x20),DAT_20007e48 / 10,0);
  DAT_20007c40 = (int)uVar2;
  DAT_20007c44 = (int)((ulonglong)uVar2 >> 0x20);
  return in_r3;
}


// ==== Counter_ComputePeriod @ 0800b67c

/* from Counter_PulseWidth: 64-bit 0x20007c48 = gate 0x20007e48 * 100000 / ((clock 0x20007e18 /
   1000) * count 0x20007e40). medium */

void Counter_ComputePeriod(void)

{
  ulonglong uVar1;
  longlong lVar2;
  uint uVar3;
  ulonglong uVar4;
  undefined8 uVar5;
  
  uVar3 = DAT_20007e40;
  uVar1 = (ulonglong)DAT_20007e48;
  uVar4 = __aeabi_uldivmod(DAT_20007e18,DAT_20007e18 >> 0x1f,1000,0);
  lVar2 = (uVar4 & 0xffffffff) * (ulonglong)uVar3;
  uVar5 = __aeabi_uldivmod((int)(uVar1 * 100000),(int)(uVar1 * 100000 >> 0x20),(int)lVar2,
                           (int)(uVar4 >> 0x20) * uVar3 + (int)((ulonglong)lVar2 >> 0x20));
  DAT_20007c48 = (int)uVar5;
  DAT_20007c4c = (int)((ulonglong)uVar5 >> 0x20);
  return;
}


// ==== Counter_ComputeDuty @ 0800b6d4

/* 0x20007e34 = count 0x20007e44 / (gate 0x20007e48 / 1000), 64-bit divide: high-time ratio in
   per-mille units; Counter_Duty shows it directly, Counter_PulseWidth scales the period by it
   /1000. medium */

void Counter_ComputeDuty(void)

{
  DAT_20007e34 = (int)((ulonglong)DAT_20007e44 / ((ulonglong)DAT_20007e48 / 1000));
  return;
}


// ==== Counter_CalibrateClock @ 0800b6f0

/* Hz gain calibration: clock 0x20007e18 = gate 0x20007e48 * 150,000,000 / (count 0x20007e40 * 100),
   i.e. scaled so the reference input reads 150,000,000 raw; Cal_StoreField(1). Clears 0x2000808e.
   medium */

undefined4 Counter_CalibrateClock(void)

{
  undefined4 in_r3;
  
  DAT_2000808e = 0;
  DAT_20007e18 = __aeabi_uldivmod((int)((ulonglong)DAT_20007e48 * 150000000),
                                  (int)((ulonglong)DAT_20007e48 * 150000000 >> 0x20),
                                  DAT_20007e40 * 100,0);
  Cal_StoreField(1,DAT_20007e18);
  return in_r3;
}


// ==== Reading_StabilityFilter @ 0800b754

/* (value, threshold, n): running mean of up to n; restart on a jump > threshold. high */

int Reading_StabilityFilter(int param_1,int param_2,ushort param_3)

{
  short sVar1;
  int iVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  bool bVar6;
  
  if ((param_2 != 0) && (param_3 != 0)) {
    uVar4 = 0;
    iVar5 = 0;
    iVar2 = param_1 - DAT_20007df8;
    if ((param_2 < iVar2) || (iVar2 + param_2 < 0 != SCARRY4(iVar2,param_2))) {
      DAT_20008085 = 0;
    }
    DAT_20007238 = param_1;
    if (DAT_20008085 == 0) {
      DAT_20008085 = 1;
      DAT_20007df8 = param_1;
    }
    else {
      DAT_20008085 = DAT_20008085 + 1;
      if (param_3 < DAT_20008085) {
        DAT_20008085 = (byte)param_3;
      }
      iVar2 = 1;
      while (sVar1 = (short)iVar2, sVar1 < (short)(ushort)DAT_20008085) {
        (&DAT_20007238)[(uint)DAT_20008085 - (int)sVar1] =
             (&DAT_20007238)[(DAT_20008085 - 1) - (int)sVar1];
        iVar2 = sVar1 + 1;
      }
      iVar2 = 0;
      while (sVar1 = (short)iVar2, sVar1 < (short)(ushort)DAT_20008085) {
        uVar3 = (&DAT_20007238)[sVar1];
        bVar6 = CARRY4(uVar4,uVar3);
        uVar4 = uVar4 + uVar3;
        iVar5 = iVar5 + ((int)uVar3 >> 0x1f) + (uint)bVar6;
        iVar2 = sVar1 + 1;
      }
      param_1 = __aeabi_ldivmod(uVar4,iVar5,DAT_20008085,0);
      DAT_20007df8 = param_1;
    }
  }
  return param_1;
}


// ==== Burden_Average @ 0800b888

/* (value) burden-voltage smoothing for AC/DC current (0x20007e6c): running mean of up to 3 (state
   0x20007e70/74/78), restarted on a jump > 10 counts; returns the mean. Never executed. medium */

int Burden_Average(int param_1)

{
  if ((10 < param_1 - DAT_20007e70) || (param_1 - DAT_20007e70 < -10)) {
    DAT_20007e78 = 0;
  }
  if (DAT_20007e78 == 0) {
    DAT_20007e78 = 1;
    DAT_20007e70 = param_1;
    DAT_20007e74 = param_1;
  }
  else {
    DAT_20007e78 = DAT_20007e78 + 1;
    if (3 < DAT_20007e78) {
      DAT_20007e78 = 3;
      DAT_20007e74 = DAT_20007e74 - DAT_20007e70;
    }
    DAT_20007e74 = param_1 + DAT_20007e74;
    DAT_20007e70 = DAT_20007e74 / DAT_20007e78;
  }
  return DAT_20007e70;
}


// ==== HY_SetMode @ 0800b974

/* HY3131 for (mode, range): HY_BuildConfig, write it (0800beac), HY_CounterRearm. First call:
   BootScreen (5, 0). high */

undefined4 HY_SetMode(undefined1 param_1,undefined1 param_2,undefined4 param_3,undefined4 param_4)

{
  HY_BuildConfig(param_1,param_2);
  HY_SetIrqMask(param_1);
  HY_CounterRearm();
  return param_4;
}


// ==== HY_BuildConfig @ 0800b994

/* 20-byte HY3131 config for (mode, range, flags) [video] */

void HY_BuildConfig(byte param_1,uint param_2)

{
  uint uVar1;
  uint uVar2;
  byte bVar3;
  byte local_30 [10];
  byte local_26;
  byte bStack_22;
  
  uVar1 = 0;
  do {
    if (0x13 < (uVar1 & 0xff)) {
      if (((param_1 == 9) && ((param_2 & 0xff) == 6)) && (g_variant_ohm6 != '\0')) {
        local_26 = local_26 & 0xf7;
      }
      DAT_2000807d = local_30[0];
      HY_Write(local_30,0x20,0x14);
      return;
    }
    if (param_1 == 0) {
      if (DAT_20008098 == '\0') {
        local_30[uVar1 & 0xff] = (&UNK_080288dc)[uVar1 & 0xff];
      }
      else {
        local_30[uVar1 & 0xff] = (&UNK_08028a04)[uVar1 & 0xff];
        if ((uVar1 & 0xff) == 10) {
          local_30[uVar1 & 0xff] = local_30[uVar1 & 0xff] | 0x20;
        }
        if ((uVar1 & 0xff) == 0xe) {
          bStack_22 = bStack_22 & 0x7f | 0x20;
        }
      }
      uVar1 = uVar1 & 0xff;
    }
    else if (param_1 == 2) {
      if (DAT_200080ba == '\0') {
        if (g_lpf_1khz == '\0') {
          if (DAT_200080b7 == '\0') {
            local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
          }
          else {
            local_30[uVar1 & 0xff] = (&UNK_08028904)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
          }
        }
        else {
          local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
        }
      }
      else if (DAT_200080bd == '\0') {
        local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
      }
      else {
        local_30[uVar1 & 0xff] = (&DAT_080288a0)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
      }
      uVar1 = uVar1 & 0xff;
      param_2 = param_2 & 0xff;
    }
    else if (param_1 < 2) {
      param_2 = param_2 & 0xff;
      uVar2 = uVar1 & 0xff;
      uVar1 = uVar1 & 0xff;
      local_30[uVar1] = (&DAT_080288a0)[uVar2 + param_2 * 0x14];
    }
    else if (param_1 == 4) {
      if (g_lpf_1khz == '\0') {
        if (DAT_200080b7 == '\0') {
          local_30[uVar1 & 0xff] = (&UNK_08028f44)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
        }
        else {
          local_30[uVar1 & 0xff] = (&UNK_08028f6c)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
        }
      }
      else {
        local_30[uVar1 & 0xff] = (&UNK_08028f44)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
      }
      uVar1 = uVar1 & 0xff;
      param_2 = param_2 & 0xff;
    }
    else if (param_1 < 4) {
      param_2 = param_2 & 0xff;
      uVar2 = uVar1 & 0xff;
      uVar1 = uVar1 & 0xff;
      local_30[uVar1] = (&DAT_08028f1c)[uVar2 + param_2 * 0x14];
    }
    else if (param_1 == 6) {
LAB_0800bdde:
      uVar2 = uVar1 & 0xff;
      uVar1 = uVar1 & 0xff;
      local_30[uVar1] = (&DAT_08028fbc)[uVar2];
    }
    else if (param_1 < 6) {
      uVar2 = uVar1 & 0xff;
      uVar1 = uVar1 & 0xff;
      local_30[uVar1] = (&DAT_08028f1c)[uVar2];
    }
    else {
      if ((param_1 == 8) || (param_1 < 8)) goto LAB_0800bdde;
      if (param_1 == 10) {
        uVar2 = uVar1 & 0xff;
        uVar1 = uVar1 & 0xff;
        local_30[uVar1] = (&DAT_08029150)[uVar2];
      }
      else if (param_1 < 10) {
        param_2 = param_2 & 0xff;
        uVar2 = uVar1 & 0xff;
        uVar1 = uVar1 & 0xff;
        local_30[uVar1] = (&DAT_080285bc)[uVar2 + param_2 * 0x14];
      }
      else if (param_1 == 0xc) {
        if (g_pf8_strap_low == '\0') {
          local_30[uVar1 & 0xff] = (&DAT_08028748)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
        }
        else {
          local_30[uVar1 & 0xff] = (&DAT_080286d0)[(uVar1 & 0xff) + (param_2 & 0xff) * 0x14];
        }
        uVar1 = uVar1 & 0xff;
        param_2 = param_2 & 0xff;
      }
      else if (param_1 < 0xc) {
        param_2 = param_2 & 0xff;
        uVar2 = uVar1 & 0xff;
        uVar1 = uVar1 & 0xff;
        local_30[uVar1] = (&LAB_08028f94)[uVar2 + param_2 * 0x14];
      }
      else if (param_1 == 0xe) {
        if (g_tdm_channel == '\0') {
          local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14];
        }
        else {
          local_30[uVar1 & 0xff] = (&DAT_08028d1c)[(uVar1 & 0xff) + (uint)DAT_200080c7 * 0x14];
        }
        uVar1 = uVar1 & 0xff;
      }
      else if (param_1 < 0xe) {
        if (g_tdm_channel == '\0') {
          local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14];
        }
        else {
          local_30[uVar1 & 0xff] = (&DAT_08028d1c)[(uVar1 & 0xff) + (uint)DAT_200080c7 * 0x14];
        }
        uVar1 = uVar1 & 0xff;
      }
      else if (param_1 == 0x10) {
LAB_0800be2c:
        param_2 = param_2 & 0xff;
        uVar2 = uVar1 & 0xff;
        uVar1 = uVar1 & 0xff;
        local_30[uVar1] = (&DAT_08028d1c)[uVar2 + param_2 * 0x14];
      }
      else if (param_1 < 0x10) {
        if (g_tdm_channel == '\0') {
          local_30[uVar1 & 0xff] = (&DAT_080289c8)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14];
        }
        else {
          bVar3 = 0;
          if (DAT_200080c7 != 0) {
            bVar3 = 2;
          }
          local_30[uVar1 & 0xff] = (&DAT_08028d1c)[(uVar1 & 0xff) + (uint)bVar3 * 0x14];
        }
        uVar1 = uVar1 & 0xff;
      }
      else {
        if (param_1 == 0x12) goto LAB_0800be2c;
        if (0x11 < param_1) {
          if (param_1 == 0x14) goto LAB_0800be2c;
          if (0x13 < param_1) {
            if (param_1 == 0x16) {
              if (g_tdm_channel == '\0') {
                local_30[uVar1 & 0xff] = (&DAT_080288a0)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14]
                ;
              }
              else {
                local_30[uVar1 & 0xff] = (&DAT_08028d58)[(uVar1 & 0xff) + (uint)DAT_200080c7 * 0x14]
                ;
              }
              uVar1 = uVar1 & 0xff;
            }
            else {
              if (param_1 < 0x16) goto LAB_0800be46;
              if (param_1 == 0x18) {
                if (g_tdm_channel == '\0') {
                  local_30[uVar1 & 0xff] =
                       (&DAT_080288a0)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14];
                }
                else {
                  bVar3 = 0;
                  if (DAT_200080c7 != 0) {
                    bVar3 = 2;
                  }
                  local_30[uVar1 & 0xff] = (&DAT_08028d58)[(uVar1 & 0xff) + (uint)bVar3 * 0x14];
                }
                uVar1 = uVar1 & 0xff;
              }
              else if (param_1 < 0x18) {
                if (g_tdm_channel == '\0') {
                  local_30[uVar1 & 0xff] =
                       (&DAT_080288a0)[(uVar1 & 0xff) + (uint)DAT_200080c6 * 0x14];
                }
                else {
                  local_30[uVar1 & 0xff] =
                       (&DAT_08028d58)[(uVar1 & 0xff) + (uint)DAT_200080c7 * 0x14];
                }
                uVar1 = uVar1 & 0xff;
              }
            }
            goto LAB_0800be5e;
          }
        }
LAB_0800be46:
        param_2 = param_2 & 0xff;
        uVar2 = uVar1 & 0xff;
        uVar1 = uVar1 & 0xff;
        local_30[uVar1] = (&DAT_08028d94)[uVar2 + param_2 * 0x14];
      }
    }
LAB_0800be5e:
    uVar1 = uVar1 + 1;
  } while( true );
}


// ==== HY_SetIrqMask @ 0800beac

/* INTF = 0 and INTE by mode (AD1, AD1+CT, AD1+RMS, CT). high */

undefined8 HY_SetIrqMask(byte param_1)

{
  undefined1 local_10;
  undefined1 local_f;
  undefined6 uStack_e;
  
  local_10 = 0;
  if (param_1 == 0) {
    if (DAT_20008098 == '\0') {
      local_f = 4;
    }
    else {
      local_f = 5;
    }
    goto LAB_0800bf6e;
  }
  if (param_1 == 2) {
    if ((DAT_200080ba != '\0') && (DAT_200080bd != '\0')) {
      local_f = 4;
      goto LAB_0800bf6e;
    }
LAB_0800bf38:
    if (DAT_200080b7 == '\x01') {
      local_f = 0x11;
      goto LAB_0800bf6e;
    }
LAB_0800bf48:
    local_f = 5;
  }
  else {
    if (param_1 < 2) goto LAB_0800bf50;
    if (param_1 == 4) goto LAB_0800bf38;
    if (param_1 < 4) goto LAB_0800bf50;
    if (param_1 == 6) {
LAB_0800bf68:
      local_f = 1;
      goto LAB_0800bf6e;
    }
    if (param_1 < 6) goto LAB_0800bf50;
    if ((param_1 == 8) || (param_1 < 8)) goto LAB_0800bf68;
    if ((param_1 == 10) || (param_1 < 10)) goto LAB_0800bf50;
    if (param_1 == 0xc) {
      local_f = 1;
      goto LAB_0800bf6e;
    }
    if (0xb < param_1) {
      if ((((param_1 == 0xe) || (param_1 < 0xe)) || (param_1 == 0x10)) ||
         ((param_1 < 0x10 || (param_1 == 0x12)))) goto LAB_0800bf48;
      if (0x11 < param_1) {
        if (param_1 == 0x14) goto LAB_0800bf48;
        if ((0x13 < param_1) && (param_1 != 0x16)) {
          if (param_1 < 0x16) {
            local_f = 4;
            goto LAB_0800bf6e;
          }
          if ((param_1 != 0x18) && (0x17 < param_1)) goto LAB_0800bf6e;
        }
      }
    }
LAB_0800bf50:
    local_f = 4;
  }
LAB_0800bf6e:
  HY_Write(&local_10,0x1e,2);
  return CONCAT62(uStack_e,CONCAT11(local_f,local_10));
}


// ==== HY_CounterRearm @ 0800bf8c

/* counter re-arm (reg 0x20 bit 1 toggle, preset 0x1B) [video] */

uint HY_CounterRearm(void)

{
  undefined2 uVar1;
  uint uVar2;
  undefined4 unaff_r7;
  undefined1 *puVar3;
  undefined1 auStack_88 [128];
  
  puVar3 = auStack_88 + 0x80;
  HY_Write(puVar3,0x20,1);
  Delay_Loop(10000);
  uVar1 = (undefined2)DAT_2000001c;
  HY_Write(puVar3,0x1b,3);
  HY_Read(&g_hy_mirror,0x1b,3);
  uVar2 = CONCAT31(CONCAT12((char)((uint)unaff_r7 >> 0x18),uVar1),DAT_2000807d);
  HY_Write(puVar3,0x20,1);
  return uVar2 | 2;
}


// ==== HY_Write @ 0800c03c  [GPIOF]

/* (buf, addr, n): DI high, CK low, CS low, delay(10), addr<<1, n bytes (auto-increment), CS high.
   high (traced + emulated) */

void HY_Write(int param_1,char param_2,uint param_3)

{
  uint uVar1;
  
  GPIOF.BSRR._0_2_ = 4;
  GPIOF.BSRR._2_2_ = 1;
  GPIOF.BSRR._2_2_ = 2;
  Delay_Loop(10);
  HY_ShiftOut(param_2 << 1,0);
  Delay_Loop(10);
  for (uVar1 = 0; (uVar1 & 0xff) < (param_3 & 0xffff); uVar1 = (uVar1 & 0xff) + 1) {
    HY_ShiftOut(*(undefined1 *)(param_1 + (uVar1 & 0xff)),0);
    Delay_Loop(10);
  }
  GPIOF.BSRR._0_2_ = 2;
  return;
}


// ==== HY_Read @ 0800c0c0  [GPIOF]

/* (addr, n): addr<<1|1 + blank clock, n bytes into the mirror 0x200077d0. high (traced + emulated)
    */

undefined4 HY_Read(undefined4 param_1,char param_2,uint param_3,undefined4 param_4)

{
  undefined1 uVar1;
  uint uVar2;
  
  GPIOF.BSRR._0_2_ = 4;
  GPIOF.BSRR._2_2_ = 1;
  GPIOF.BSRR._2_2_ = 2;
  Delay_Loop(5);
  HY_ShiftOut(param_2 << 1 | 1,1);
  Delay_Loop(5);
  uVar2 = 0;
  while( true ) {
    if ((param_3 & 0xffff) <= (uVar2 & 0xff)) break;
    uVar1 = HY_ShiftIn();
    (&g_hy_mirror)[uVar2 & 0xff] = uVar1;
    Delay_Loop(2);
    uVar2 = (uVar2 & 0xff) + 1;
  }
  GPIOF.BSRR._0_2_ = 2;
  return param_4;
}


// ==== HY_ShiftIn @ 0800c138  [GPIOF]

/* CK high, sample PF3 (DO), CK low; MSB first. high */

uint HY_ShiftIn(void)

{
  ushort uVar1;
  uint uVar2;
  uint uVar3;
  
  uVar2 = 0;
  GPIOF.BSRR._2_2_ = 1;
  for (uVar3 = 0; uVar3 = uVar3 & 0xff, uVar3 < 8; uVar3 = uVar3 + 1) {
    GPIOF.BSRR._0_2_ = 1;
    uVar1 = GPIOF.IDR._0_2_;
    if ((int)((uint)uVar1 << 0x1c) < 0) {
      uVar2 = uVar2 | 0x80 >> uVar3;
    }
    GPIOF.BSRR._2_2_ = 1;
  }
  return uVar2 & 0xff;
}


// ==== HY_ShiftOut @ 0800c16c  [GPIOF]

/* byte MSB first: DI then CK pulse; arg 1 adds the read's blank clock. high */

void HY_ShiftOut(byte param_1,char param_2)

{
  uint uVar1;
  
  for (uVar1 = 0; uVar1 = uVar1 & 0xff, uVar1 < 8; uVar1 = uVar1 + 1) {
    if (((uint)param_1 & 0x80 >> uVar1) == 0) {
      GPIOF.BSRR._2_2_ = 4;
    }
    else {
      GPIOF.BSRR._0_2_ = 4;
    }
    HY_ClockPulse();
  }
  if (param_2 == '\x01') {
    GPIOF.BSRR._2_2_ = 4;
    HY_ClockPulse();
  }
  GPIOF.BSRR._0_2_ = 4;
  return;
}


// ==== HY_ClockPulse @ 0800c1bc  [GPIOF]

/* CK high then low. medium */

undefined4 HY_ClockPulse(void)

{
  undefined4 unaff_r7;
  
  Delay_Loop(1);
  GPIOF.BSRR._0_2_ = 1;
  Delay_Loop(1);
  GPIOF.BSRR._2_2_ = 1;
  return unaff_r7;
}


// ==== Board_ProbePF8 @ 0800c28c  [GPIOF]

/* PF8 input pull-up, 20 reads; low on >= 11 of them -> 0x20007fdf = 1; then PF8 analog. PF8
   unlabeled in the CubeMX pin map: board-variant strap? medium */

undefined8 Board_ProbePF8(void)

{
  ushort uVar1;
  byte bVar2;
  byte bVar3;
  undefined4 local_18;
  undefined1 local_14;
  undefined1 local_13;
  undefined1 uStack_12;
  undefined1 local_11;
  
  bVar3 = 0;
  g_pf8_strap_low = 0;
  local_18 = 0x100;
  local_14 = 0;
  local_11 = 1;
  local_13 = 3;
  GPIO_Init(&GPIOF,&local_18);
  for (bVar2 = 0; bVar2 < 0x14; bVar2 = bVar2 + 1) {
    uVar1 = GPIOF.IDR._0_2_;
    if ((int)((uint)uVar1 << 0x17) < 0) {
      bVar3 = bVar3 + 1;
    }
  }
  if (bVar3 < 10) {
    g_pf8_strap_low = 1;
  }
  local_18 = 0x100;
  local_13 = 3;
  local_11 = 0;
  local_14 = 3;
  GPIO_Init(&GPIOF,&local_18);
  return CONCAT17(local_11,CONCAT16(uStack_12,CONCAT15(local_13,CONCAT14(local_14,local_18))));
}


// ==== BLE_SendReply @ 0800c308  [USART2]

/* echoes the received frame (0x2000802f bytes from 0x20007b94). high */

void BLE_SendReply(void)

{
  uint uVar1;
  
  for (uVar1 = 0; (uVar1 & 0xff) < (uint)DAT_2000802f; uVar1 = (uVar1 & 0xff) + 1) {
    USART_SendByteBlocking(&USART2,(&DAT_20007b94)[uVar1 & 0xff]);
  }
  return;
}


// ==== BLE_ProcessRx @ 0800c332

/* main loop: copies g_ble_rx (0x2000802f bytes) to 0x20007b94 and parses ASCII hex pairs (0800c470,
   upper case only). 0xF8: six bytes + XOR check -> BLE_SendReply, saved to 0x200080d7..dc and
   written to the RTC by 0801016c once (latch 0x20008033), bad check clears 0x20008034. 0xF4: key
   byte sent twice, must match -> reply, 0x2000806d key, 0x20008069 pending (Key_FromBLE). high */

byte BLE_ProcessRx(void)

{
  byte unaff_10000084;
  byte bVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  byte bVar5;
  byte bVar6;
  char cVar7;
  char cVar8;
  byte in_r3;
  uint uVar9;
  
  DAT_20008030 = 0;
  for (uVar9 = 0; (uVar9 & 0xff) < (uint)DAT_2000802f; uVar9 = (uVar9 & 0xff) + 1) {
    (&DAT_20007b94)[uVar9 & 0xff] = (&g_ble_rx)[uVar9 & 0xff];
  }
  if (DAT_20007b94 == -8) {
    in_r3 = BLE_HexPairToByte(1);
    bVar1 = BLE_HexPairToByte(3);
    bVar2 = BLE_HexPairToByte(5);
    bVar3 = BLE_HexPairToByte(7);
    bVar4 = BLE_HexPairToByte(9);
    bVar5 = BLE_HexPairToByte(0xb);
    bVar6 = BLE_HexPairToByte(0xd);
    if ((byte)(bVar5 ^ bVar4 ^ bVar3 ^ bVar2 ^ bVar1 ^ in_r3) == bVar6) {
      BLE_SendReply();
      if (DAT_20008033 == '\0') {
        DAT_200080d7 = in_r3;
        DAT_200080d8 = bVar1;
        DAT_200080d9 = bVar2;
        DAT_200080da = bVar3;
        DAT_200080db = bVar4;
        DAT_200080dc = bVar5;
        RTC_WriteClock();
        DAT_20008033 = '\x01';
      }
    }
    else {
      DAT_20008034 = 0;
    }
  }
  else if (DAT_20007b94 == -0xc) {
    cVar7 = BLE_HexPairToByte(1);
    cVar8 = BLE_HexPairToByte(3);
    in_r3 = unaff_10000084;
    if (cVar7 == cVar8) {
      BLE_SendReply();
      DAT_20008069 = 1;
      DAT_2000806d = cVar7;
    }
  }
  return in_r3;
}


// ==== BLE_HexPairToByte @ 0800c470

/* (offset) two ASCII hex digits at 0x20007b94[offset..+1] -> byte; '0'-'9' and 'A'-'F' only (lower
   case or junk counts 0). From BLE_ProcessRx. high (emulated) */

char BLE_HexPairToByte(byte param_1)

{
  char cVar1;
  
  cVar1 = '\0';
  if (((byte)(&DAT_20007b94)[param_1] < 0x30) || (0x39 < (byte)(&DAT_20007b94)[param_1])) {
    if ((0x40 < (byte)(&DAT_20007b94)[param_1]) && ((byte)(&DAT_20007b94)[param_1] < 0x47)) {
      cVar1 = ((&DAT_20007b94)[param_1] + -0x37) * '\x10';
    }
  }
  else {
    cVar1 = (&DAT_20007b94)[param_1] << 4;
  }
  if (((byte)(&DAT_20007b95)[param_1] < 0x30) || (0x39 < (byte)(&DAT_20007b95)[param_1])) {
    if ((0x40 < (byte)(&DAT_20007b95)[param_1]) && ((byte)(&DAT_20007b95)[param_1] < 0x47)) {
      cVar1 = (&DAT_20007b95)[param_1] + -0x37 + cVar1;
    }
  }
  else {
    cVar1 = (&DAT_20007b95)[param_1] + -0x30 + cVar1;
  }
  return cVar1;
}


// ==== BLE_Toggle @ 0800c51a

/* start or stop per 0x2000802b, redraw; only caller Key_LongPress: BLE never starts by itself. high
    */

void BLE_Toggle(void)

{
  if (g_ble_on == '\0') {
    BLE_Stop();
  }
  else {
    BLE_Start();
  }
  LCD_DrawBtIcon(1);
  return;
}


// ==== BLE_PacketHeader @ 0800c53a

/* from BLE_BuildPacket: bytes 1..4 of g_ble_tx from g_date_stamp and g_meter_id as packed BCD
   digits (via 0801b090 digit split). medium */

undefined4 BLE_PacketHeader(void)

{
  undefined4 unaff_r7;
  
  Num_SplitDigits(g_date_stamp);
  DAT_20007ae1 = ((byte)DAT_20007db8 & 0xf) + (char)(DAT_20007db4 << 4);
  DAT_20007ae2 = ((char)DAT_20007dbc * '\n' + (char)DAT_20007dc0) * '\x10';
  Num_SplitDigits(g_meter_id);
  DAT_20007ae2 = ((byte)DAT_20007db0 & 0xf) + DAT_20007ae2;
  DAT_20007ae3 = ((byte)DAT_20007db8 & 0xf) + (char)(DAT_20007db4 << 4);
  DAT_20007ae4 = ((byte)DAT_20007dc0 & 0xf) + (char)(DAT_20007dbc << 4);
  return unaff_r7;
}


// ==== BLE_BuildPacket_Thunk @ 0800c5d2

/* main: thunk to BLE_BuildPacket. high */

undefined4 BLE_BuildPacket_Thunk(void)

{
  undefined4 unaff_r7;
  
  BLE_BuildPacket();
  return unaff_r7;
}


// ==== BLE_BuildPacket @ 0800c5da

/* 19-byte packet in g_ble_tx: 0xF2, 0800c53a header, mode (+0x80/+0x40 = bits 17/16 of |value|),
   range (+0x80 0x20008048, +0x40 negative, temp C/F +0x10/+0x20, diode 15 V -> 1), value low 16
   bits; also appends the value to a 1000-entry ring 0x20003950 (index 0x20007fb0); sub-display
   fields by g_setup_page (0800c99e.. pages 0-12); status bytes (LPF, AC/DC, APO, BLE, battery); XOR
   of bytes 0-17 at byte 18. high [video] (RE_NOTES 0x0800C5DB, confirmed) */

void BLE_BuildPacket(void)

{
  char cVar1;
  int iVar2;
  uint uVar3;
  
  g_ble_tx = 0xf2;
  BLE_PacketHeader();
  if (DAT_20007d70 < 0) {
    iVar2 = -DAT_20007d70;
  }
  else {
    iVar2 = DAT_20007d70;
  }
  DAT_20007ae5 = g_mode;
  if (iVar2 << 0xe < 0) {
    DAT_20007ae5 = g_mode + 0x80;
  }
  if (iVar2 << 0xf < 0) {
    DAT_20007ae5 = DAT_20007ae5 + 0x40;
  }
  DAT_20007ae6 = g_range;
  if ((g_mode == 0xb) && (g_diode_15v != '\0')) {
    DAT_20007ae6 = '\x01';
  }
  if (DAT_20008048 != '\0') {
    DAT_20007ae6 = DAT_20007ae6 + -0x80;
  }
  if (DAT_20007d70 < 0) {
    DAT_20007ae6 = DAT_20007ae6 + '@';
  }
  if (g_mode == 5) {
    if (g_temp_unit == '\0') {
      DAT_20007ae6 = DAT_20007ae6 + '\x10';
    }
    else {
      DAT_20007ae6 = DAT_20007ae6 + ' ';
    }
  }
  if (DAT_20007d70 < 0) {
    iVar2 = -DAT_20007d70;
  }
  else {
    iVar2 = DAT_20007d70;
  }
  DAT_20007ae7 = (undefined1)((uint)iVar2 >> 8);
  DAT_20007ae8 = (undefined1)iVar2;
  DAT_20007fb0 = DAT_20007fb0 + 1;
  if (999 < DAT_20007fb0) {
    DAT_20007fb0 = 0;
  }
  (&DAT_20003950)[DAT_20007fb0] = DAT_20007d70;
  if (g_setup_page == '\0') {
    BLE_SubTemperature(9);
  }
  else if (g_setup_page == '\x01') {
    BLE_SubBattery(9);
  }
  else if (g_setup_page == '\x02') {
    BLE_SubApo(9);
  }
  else if (g_setup_page == '\x04') {
    BLE_SubContrast(9);
  }
  else if (g_setup_page == '\x05') {
    BLE_SubYear(9);
  }
  else if (g_setup_page == '\x06') {
    BLE_SubDate(9);
  }
  else if (g_setup_page == '\a') {
    BLE_SubTime(9);
  }
  else if (g_setup_page == '\n') {
    BLE_SubDisplayPage10(9);
  }
  else if (g_setup_page == '\v') {
    BLE_SubDisplayPage11(9);
  }
  else if (g_setup_page == '\f') {
    BLE_SubBurden_Thunk(9);
  }
  else {
    BLE_SubBlank(9);
  }
  if ((((5 < g_mode) && (g_mode < 9)) || (g_mode == 0xc)) ||
     (DAT_20007aed = DAT_2000804d, g_mode == 5)) {
    DAT_20007aed = DAT_2000804d + '\x10';
  }
  if (g_reading_fast < 0) {
    DAT_20007aed = DAT_20007aed + '\x04';
  }
  if (DAT_2000804d == '\x03') {
    DAT_20007aed = DAT_20007aed + '\b';
  }
  DAT_20007aee = DAT_20008041;
  if (g_lpf_1khz == '\0') {
    cVar1 = '\0';
  }
  else {
    cVar1 = '@';
  }
  if (DAT_200080b7 != '\0') {
    cVar1 = cVar1 + ' ';
  }
  if (DAT_200080ba == '\0') {
    if ((((g_mode == 0x10) || (g_mode == 0x12)) ||
        ((g_mode == 0x14 || ((g_mode == 2 || (g_mode == 4)))))) ||
       ((g_mode == 0xd || ((g_mode == 0xe || (g_mode == 0xf)))))) {
      cVar1 = cVar1 + '\x10';
    }
    else if ((((g_mode == 0x11) || (((g_mode == 0x13 || (g_mode == 0x15)) || (g_mode == 1)))) ||
             (((g_mode == 3 || (g_mode == 0x16)) || (g_mode == 0x17)))) || (g_mode == 0x18)) {
      cVar1 = cVar1 + '\b';
    }
  }
  else {
    cVar1 = cVar1 + '\x18';
  }
  iVar2 = Range_IsAuto();
  if (iVar2 != 0) {
    cVar1 = cVar1 + '\x04';
  }
  if (g_apo_off == '\0') {
    cVar1 = cVar1 + '\x02';
  }
  if (DAT_2000803b != '\0') {
    cVar1 = cVar1 + '\x01';
  }
  if (g_ble_on == '\0') {
    DAT_20007af0 = '\0';
  }
  else {
    DAT_20007af0 = '@';
  }
  if (DAT_200080b1 != '\0') {
    DAT_20007af0 = DAT_20007af0 + ' ';
  }
  if (DAT_20008058 != '\0') {
    DAT_20007af0 = DAT_20007af0 + '\x10';
  }
  if ((g_mode == 2) && (g_setup_page == '\v')) {
    DAT_20007af0 = DAT_20007af0 + '\b';
  }
  if (DAT_200080b7 == '\0') {
    if (DAT_2000804e != '\0') {
      DAT_20007af0 = DAT_2000804e + DAT_20007af0;
    }
  }
  else if (DAT_200080b8 == '\0') {
    DAT_20007af0 = DAT_20007af0 + '\x01';
  }
  else {
    DAT_20007af0 = DAT_20007af0 + '\x02';
  }
  DAT_20007aef = cVar1;
  cVar1 = Hold_State();
  DAT_20007af1 = cVar1 * '\x04';
  if ((DAT_20007fe1 & DAT_20007fe3) != 0 || DAT_20007fe4 != '\0') {
    DAT_20007af1 = DAT_20007af1 + '\x10';
  }
  DAT_20008032 = g_ble_tx;
  for (uVar3 = 1; (uVar3 & 0xff) < 0x12; uVar3 = (uVar3 & 0xff) + 1) {
    DAT_20008032 = DAT_20008032 ^ (&g_ble_tx)[uVar3 & 0xff];
  }
  DAT_20007af2 = DAT_20008032;
  return;
}


// ==== BLE_SubTemperature @ 0800c99e

/* (offset) sub-display field: code 0x69 (g_temp_unit 0, deg F) or 0x64 (C), flags 1 (+0x80 if
   0x200080ae, +0x40 negative), value 08018ad8(0x20007fcc) (0.1 deg rounded) as 16-bit magnitude
   (0800d100). high */

void BLE_SubTemperature(byte param_1)

{
  short sVar1;
  undefined1 uVar2;
  char cVar3;
  
  if (g_temp_unit == '\0') {
    uVar2 = 0x69;
  }
  else {
    uVar2 = 100;
  }
  (&g_ble_tx)[param_1] = uVar2;
  sVar1 = Temp_ToDisplayTenths((int)DAT_20007fcc);
  cVar3 = '\x01';
  if (DAT_200080ae != '\0') {
    cVar3 = -0x7f;
  }
  if (sVar1 < 0) {
    cVar3 = cVar3 + '@';
  }
  (&DAT_20007ae1)[param_1] = cVar3;
  BLE_PutU16(param_1 + 2 & 0xff,(int)sVar1);
  return;
}


// ==== BLE_SubBattery @ 0800c9fc

/* (offset) BLE sub field for SETUP page 1: code 0x6E, flags 1, value g_battery_dV (BLE_PutU16).
   high */

undefined4 BLE_SubBattery(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  (&g_ble_tx)[param_1] = 0x6e;
  (&DAT_20007ae1)[param_1] = 1;
  BLE_PutU16(param_1 + 2 & 0xff,g_battery_dV);
  return param_4;
}


// ==== BLE_SubApo @ 0800ca2a

/* (offset) BLE sub field for SETUP page 2: code 0x78 (APO on, g_apo_off 0) or 0x7D (off), flags 2,
   value 0. Never executed. high */

undefined4 BLE_SubApo(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined1 uVar1;
  
  if (g_apo_off == '\0') {
    uVar1 = 0x78;
  }
  else {
    uVar1 = 0x7d;
  }
  (&g_ble_tx)[param_1] = uVar1;
  (&DAT_20007ae1)[param_1] = 2;
  BLE_PutU16(param_1 + 2 & 0xff,0);
  return param_4;
}


// ==== BLE_SubContrast @ 0800ca64

/* (offset) BLE sub field for SETUP page 4: code 0xA0, flags 0, value g_lcd_contrast - 10. Never
   executed. high */

undefined4 BLE_SubContrast(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  (&g_ble_tx)[param_1] = 0xa0;
  (&DAT_20007ae1)[param_1] = 0;
  BLE_PutU16(param_1 + 2 & 0xff,g_lcd_contrast - 10);
  return param_4;
}


// ==== BLE_SubYear @ 0800ca96

/* (offset) BLE sub field for SETUP page 5: code 0x82, flags 0, value year 0x200080d7 (two digits).
   Never executed. high */

undefined4 BLE_SubYear(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  (&g_ble_tx)[param_1] = 0x82;
  (&DAT_20007ae1)[param_1] = 0;
  BLE_PutU16(param_1 + 2 & 0xff,DAT_200080d7);
  return param_4;
}


// ==== BLE_SubDate @ 0800cac4

/* (offset) sub-display field: code 0x87, flags 0, month 0x200080d8, day 0x200080d9. Never executed.
   medium */

void BLE_SubDate(byte param_1)

{
  (&g_ble_tx)[param_1] = 0x87;
  (&DAT_20007ae1)[param_1] = 0;
  (&DAT_20007ae2)[param_1] = DAT_200080d8;
  (&DAT_20007ae3)[param_1] = DAT_200080d9;
  return;
}


// ==== BLE_SubTime @ 0800cb04

/* (offset) sub-display field: code 0x8C, flags 0, hour 0x200080da, minute 0x200080db. Never
   executed. medium */

void BLE_SubTime(byte param_1)

{
  (&g_ble_tx)[param_1] = 0x8c;
  (&DAT_20007ae1)[param_1] = 0;
  (&DAT_20007ae2)[param_1] = DAT_200080da;
  (&DAT_20007ae3)[param_1] = DAT_200080db;
  return;
}


// ==== BLE_SubDisplayPage10 @ 0800cb44

/* BLE sub-display fields for SETUP page 10: ACmV/ACV -> frequency (0x20007d84/10, range 0x20008040,
   overflow 0x20008047, 0800ce78); continuity -> 0xAA + g_continuity_mode; diode -> unit 0x0B, value
   3 or 15 (g_diode_15v); VA modes -> 0800cefc or 0800cfb0 (by 0x200080c2); current -> 0800ce32 with
   burden flag 0x200080a5; else blank (0800d0c8). medium */

undefined4
BLE_SubDisplayPage10(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  if ((g_mode == '\x04') || ((g_mode == '\x02' && (DAT_200080ba == '\0')))) {
    BLE_SubFrequency(param_1,DAT_20008040,DAT_20008047,DAT_20007d84 / 10);
  }
  else if (g_mode == '\n') {
    (&g_ble_tx)[param_1] = g_continuity_mode + -0x56;
    (&DAT_20007ae1)[param_1] = 0;
    BLE_PutU16(param_1 + 2 & 0xff,0);
  }
  else if (g_mode == '\v') {
    (&g_ble_tx)[param_1] = 0xb;
    uVar1 = (uint)param_1;
    (&DAT_20007ae1)[uVar1] = 0;
    if (g_diode_15v == '\0') {
      BLE_PutU16(uVar1 + 2 & 0xff,3);
    }
    else {
      BLE_PutU16(uVar1 + 2 & 0xff,0xf);
    }
  }
  else if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
          (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
    if (DAT_200080c2 == '\0') {
      BLE_SubVaVolts(param_1,DAT_20008040,0);
    }
    else {
      BLE_SubVaAmps(param_1,DAT_20008040,0);
    }
  }
  else if (((g_mode == '\x10') || (g_mode == '\x12')) ||
          ((g_mode == '\x14' || (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))))
          ) {
    BLE_SubBurdenFlag(param_1,DAT_200080a5);
  }
  else {
    BLE_SubBlank(param_1);
  }
  return param_4;
}


// ==== BLE_SubDisplayPage11 @ 0800ccc8

/* BLE sub-display fields for SETUP page 11: ACV -> code 0xB4 with signed value 0x20007e98 (from
   08018590, float 0x20007e94 with decimal 0x20008040; maybe dBm, unconfirmed), +0x80 overflow
   0x20008046, +0x40 negative; AC current burden off -> frequency (0800ce78); current burden on ->
   0800d07c; VA modes -> 0800ce32 with g_va_burden_display; else blank. medium */

void BLE_SubDisplayPage11(byte param_1)

{
  char cVar1;
  int iVar2;
  
  if (g_mode == '\x02') {
    (&g_ble_tx)[param_1] = 0xb4;
    cVar1 = DAT_20008040;
    if (DAT_20008046 != '\0') {
      cVar1 = DAT_20008040 + -0x80;
    }
    if (DAT_20007e98 < 0) {
      cVar1 = cVar1 + '@';
    }
    (&DAT_20007ae1)[param_1] = cVar1;
    if (DAT_20007e98 < 0) {
      iVar2 = -DAT_20007e98;
    }
    else {
      iVar2 = DAT_20007e98;
    }
    BLE_PutU16(param_1 + 2 & 0xff,iVar2);
  }
  else if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
          (DAT_200080a5 == '\0')) {
    BLE_SubFrequency(param_1,DAT_20008040,DAT_20008047,DAT_20007d84 / 10);
  }
  else if (((((g_mode == '\x10') || (g_mode == '\x12')) ||
            ((g_mode == '\x14' || ((g_mode == '\x11' || (g_mode == '\x13')))))) ||
           (g_mode == '\x15')) && (DAT_200080a5 != '\0')) {
    BLE_SubBurden();
  }
  else if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
           ((g_mode == '\x16' || (g_mode == '\x17')))) || (g_mode == '\x18')) {
    BLE_SubBurdenFlag(param_1,g_va_burden_display);
  }
  else {
    BLE_SubBlank(param_1);
  }
  return;
}


// ==== BLE_SubBurden_Thunk @ 0800ce28

/* BLE_BuildPacket SETUP page 12: thunk to BLE_SubBurden (offset arg passed through, fixed 9
   anyway). high */

void BLE_SubBurden_Thunk(void)

{
  BLE_SubBurden();
  return;
}


// ==== BLE_SubBurdenFlag @ 0800ce32

/* (offset, on) BLE sub field: code 0x9B (on) / 0x9C (off), flags 3, value 0; current burden flag
   0x200080a5 (page 10) or g_va_burden_display (page 11). medium */

void BLE_SubBurdenFlag(byte param_1,char param_2)

{
  undefined1 uVar1;
  
  if (param_2 == '\0') {
    uVar1 = 0x9c;
  }
  else {
    uVar1 = 0x9b;
  }
  (&g_ble_tx)[param_1] = uVar1;
  (&DAT_20007ae1)[param_1] = 3;
  BLE_PutU16(param_1 + 2 & 0xff,0);
  return;
}


// ==== BLE_SubFrequency @ 0800ce78

/* (offset, decimal, overflow, freq) sub-display frequency: code 6, flags decimal + 0x10 (+0x30 if
   freq range 0x2000803e > 1), +0x80 overflow, +0x40 negative; 16-bit magnitude. From
   BLE_SubDisplayPage10/11. high */

int BLE_SubFrequency(byte param_1,char param_2,char param_3,int param_4)

{
  int iVar1;
  char cVar2;
  
  (&g_ble_tx)[param_1] = 6;
  if (param_3 != '\0') {
    param_2 = param_2 + -0x80;
  }
  if (param_4 < 0) {
    param_2 = param_2 + '@';
  }
  cVar2 = param_2 + '\x10';
  if (1 < DAT_2000803e) {
    cVar2 = param_2 + '0';
  }
  (&DAT_20007ae1)[param_1] = cVar2;
  if (param_4 < 0) {
    iVar1 = -param_4;
  }
  else {
    iVar1 = param_4;
  }
  BLE_PutU16(param_1 + 2 & 0xff,iVar1);
  return param_4;
}


// ==== BLE_SubVaVolts @ 0800cefc

/* (offset, decimal, store) VA sub field for the voltage 0x20007eac: code 2 (AC VA 13-15) or 1 (DC
   VA 22-24), +0x80 if either VA overrange flag, +0x40 negative; store arg (always 0) would copy to
   0x200048f0[0x20007fb0]. medium */

void BLE_SubVaVolts(byte param_1,char param_2,char param_3)

{
  undefined1 unaff_r7;
  int iVar1;
  
  if (g_mode - 0xd < 3) {
    unaff_r7 = 2;
  }
  else if (g_mode - 0x16 < 3) {
    unaff_r7 = 1;
  }
  (&g_ble_tx)[param_1] = unaff_r7;
  if ((DAT_200080ca != '\0') || (DAT_200080cb != '\0')) {
    param_2 = param_2 + -0x80;
  }
  if (DAT_20007eac < 0) {
    param_2 = param_2 + '@';
  }
  (&DAT_20007ae1)[param_1] = param_2;
  if (DAT_20007eac < 0) {
    iVar1 = -DAT_20007eac;
  }
  else {
    iVar1 = DAT_20007eac;
  }
  BLE_PutU16(param_1 + 2 & 0xff,iVar1);
  if (param_3 != '\0') {
    *(int *)(&DAT_200048f0 + (uint)DAT_20007fb0 * 4) = DAT_20007eac;
  }
  return;
}


// ==== BLE_SubVaAmps @ 0800cfb0

/* (offset, decimal, store) VA sub field for the current 0x20007eb0: code = the current mode number
   (16/18/20 AC, 17/19/21 DC uA/mA/A) by VA mode, +0x80 overrange, +0x40 negative; store arg (always
   0) -> 0x20005890[0x20007fb0]. Never executed. medium */

void BLE_SubVaAmps(byte param_1,char param_2,char param_3)

{
  undefined1 unaff_r7;
  int iVar1;
  
  if (g_mode == '\r') {
    unaff_r7 = 0x10;
  }
  else if (g_mode == '\x0e') {
    unaff_r7 = 0x12;
  }
  else if (g_mode == '\x0f') {
    unaff_r7 = 0x14;
  }
  else if (g_mode == '\x16') {
    unaff_r7 = 0x11;
  }
  else if (g_mode == '\x17') {
    unaff_r7 = 0x13;
  }
  else if (g_mode == '\x18') {
    unaff_r7 = 0x15;
  }
  (&g_ble_tx)[param_1] = unaff_r7;
  if ((DAT_200080cb != '\0') || (DAT_200080ca != '\0')) {
    param_2 = param_2 + -0x80;
  }
  if (DAT_20007eb0 < 0) {
    param_2 = param_2 + '@';
  }
  (&DAT_20007ae1)[param_1] = param_2;
  if (DAT_20007eb0 < 0) {
    iVar1 = -DAT_20007eb0;
  }
  else {
    iVar1 = DAT_20007eb0;
  }
  BLE_PutU16(param_1 + 2 & 0xff,iVar1);
  if (param_3 != '\0') {
    *(int *)(&DAT_20005890 + (uint)DAT_20007fb0 * 4) = DAT_20007eb0;
  }
  return;
}


// ==== BLE_SubBurden @ 0800d07c

/* sub field at fixed offset 9: code 0x96, flags 1 (+0x40 negative), burden value 0x20007fca
   (LCD_SubBurden). Never executed. medium */

undefined4 BLE_SubBurden(void)

{
  int iVar1;
  undefined4 in_r3;
  
  DAT_20007ae9 = 0x96;
  DAT_20007aea = 1;
  if (DAT_20007fca < 0) {
    DAT_20007aea = 0x41;
    iVar1 = -(int)DAT_20007fca;
  }
  else {
    iVar1 = (int)DAT_20007fca;
  }
  BLE_PutU16(0xb,iVar1);
  return in_r3;
}


// ==== BLE_SubBlank @ 0800d0c8

/* (offset) empty BLE sub field: code 0, flags 0, value 0; pages without a field. high */

undefined4 BLE_SubBlank(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  (&g_ble_tx)[param_1] = 0;
  (&DAT_20007ae1)[param_1] = 0;
  BLE_PutU16(param_1 + 2 & 0xff,0);
  return param_4;
}


// ==== BLE_PutU16 @ 0800d100

/* (offset, value) |value| as big-endian 16 bits into g_ble_tx[offset], [offset+1]; used by every
   BLE sub-field writer. high */

void BLE_PutU16(byte param_1,int param_2)

{
  if (param_2 < 0) {
    param_2 = -param_2;
  }
  (&g_ble_tx)[param_1] = (char)((uint)param_2 >> 8);
  (&DAT_20007ae1)[param_1] = (char)param_2;
  return;
}


// ==== USART_SendByteBlocking @ 0800d170

/* (USARTx, byte) spin on flag 0x80 (TXE) then send; BLE_SendReply, TIM2_IRQHandler (first packet
   byte), USART2_IRQHandler. high */

undefined4
USART_SendByteBlocking(undefined4 param_1,undefined1 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  
  do {
    iVar1 = USART_GetFlagStatus(param_1,0x80);
  } while (iVar1 == 0);
  USART_SendData(param_1,param_2);
  return param_4;
}


// ==== Setup_EditTimeout @ 0800d190

/* main every 0.5 s (TIM2 sets 0x200080d3) while editing SETUP (0x2000806e): count 0x20008070 (reset
   by keys) > 240 = ~120 s idle -> edit cancelled, unsaved value restored per g_setup_page (8 meter
   id from g_meter_id; 5/6/7 RTC year/month-day/hour-minute shadows 0x200080dd..e1 from
   0x200080d7..db; 2/3/0/10 reload APO/buzzer/temp unit/continuity from EEPROM), redraw 0x20008062.
   high */

undefined4 Setup_EditTimeout(void)

{
  undefined4 unaff_r7;
  
  DAT_20008070 = DAT_20008070 + 1;
  if (0xf0 < DAT_20008070) {
    DAT_2000806e = 0;
    if (g_setup_page == '\b') {
      DAT_20007d18 = g_meter_id;
    }
    else if (g_setup_page == '\x05') {
      DAT_200080dd = DAT_200080d7;
    }
    else if (g_setup_page == '\x06') {
      DAT_200080de = DAT_200080d8;
      DAT_200080df = DAT_200080d9;
      DAT_200080d6 = 0;
    }
    else if (g_setup_page == '\a') {
      DAT_200080e0 = DAT_200080da;
      DAT_200080e1 = DAT_200080db;
      DAT_200080d6 = 0;
    }
    else if (g_setup_page == '\x02') {
      Settings_LoadApo();
    }
    else if (g_setup_page == '\x03') {
      Settings_LoadBuzzer();
    }
    else if (g_setup_page == '\0') {
      Settings_LoadTempUnit();
    }
    else if ((g_mode == '\n') && (g_setup_page == '\n')) {
      Settings_LoadContinuity();
    }
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== Mode_ApplyChange @ 0800d2d0

/* (stop_log) main when 0x2000804a (mode changed) or 0x2000804b (range changed): clears
   VA/TDM/REL/PEAK/LPF/AC+DC/hold state; on a mode change also resets SETUP page
   (Setup_InitialPage), MIN/MAX, range (0, Ohm 6, kept with PEAK), stops logging if arg 1 and
   logging (0x20007fe1); then Reading_SetSettleCount, full scale, Cal/HY reconfig
   (0800fbe8(mode,range)), APO_Reset on mode change. [video "full mode/range change", confirmed]
   medium */

void Mode_ApplyChange(char param_1)

{
  if (((g_mode == '\t') && (g_range != 0)) && (g_range < 6)) {
    DAT_2000805a = 2;
  }
  DAT_200080a4 = 0;
  DAT_2000809a = 0;
  DAT_200080c5 = 0;
  g_tdm_channel = 0;
  DAT_20007d70 = 0;
  DAT_20008081 = 0;
  DAT_20008080 = 0;
  g_va_volts = 0;
  g_va_amps = 0;
  DAT_20007e64 = 0;
  DAT_200080a1 = 0;
  if ((g_mode == '\x02') && (DAT_200080ba != '\0')) {
    DAT_20007e9c = 0;
    DAT_20007ea0 = 0;
  }
  if (DAT_2000804a != '\0') {
    DAT_200080a2 = 0;
    DAT_200080c6 = 0;
    DAT_200080c7 = 0;
    DAT_200080c2 = 0;
    DAT_200080c8 = 0;
    DAT_200080c9 = 0;
    DAT_20008042 = g_mode == '\v';
    Setup_InitialPage();
    DAT_200080a6 = 0;
    DAT_200080a5 = 0;
    DAT_2000806e = 0;
    if (((g_mode == '\x02') && (DAT_200080ba == '\0')) || (g_mode == '\x04')) {
      if (DAT_200080b7 == '\0') {
        g_range = 0;
      }
      else {
        DAT_20008042 = 1;
      }
    }
    else if ((g_mode == '\t') && (g_cal_mode == '\0')) {
      g_range = 6;
    }
    else {
      g_range = 0;
    }
    DAT_20008050 = '\0';
    DAT_20008051 = 0;
    DAT_20008052 = 0;
    DAT_20008053 = 0;
    DAT_20007fb2 = 0;
    if ((param_1 == '\x01') && (DAT_20007fe1 != '\0')) {
      Log_Stop(1);
      DAT_20007fe1 = '\0';
    }
    Display_ResetToLive();
  }
  DAT_20007fc2 = 0;
  DAT_20007fc0 = 0;
  DAT_200080b1 = 0;
  DAT_200080b2 = 0;
  if ((g_mode != '\x02') && (g_mode != '\x04')) {
    g_lpf_1khz = 0;
  }
  if (g_mode != '\x02') {
    DAT_200080ba = '\0';
  }
  DAT_20008058 = 0;
  DAT_2000804e = 0;
  if (((g_mode != '\x02') || (DAT_200080ba != '\0')) && (g_mode != '\x04')) {
    DAT_200080b7 = '\0';
  }
  DAT_20007e8c = 0;
  DAT_20007e90 = 0;
  DAT_20008063 = 1;
  DAT_20008060 = 1;
  DAT_20008062 = 1;
  DAT_2000804c = 0;
  if (DAT_2000804f == '\0' && DAT_20008050 == '\0') {
    DAT_2000805f = 1;
  }
  Reading_SetSettleCount();
  LCD_UpdateMainDP();
  Mode_SetFullScale();
  Mode_ConfigureHyCalCap(1);
  if ((g_mode != '\t') || (g_range != 6)) {
    g_reading = 0;
    g_reading_fast = 0;
  }
  VA_Reset();
  Frontend_SetAll(g_mode,g_range);
  if (DAT_2000804a != '\0') {
    APO_Reset();
    Buzzer_On(300);
  }
  DAT_2000804a = 0;
  DAT_2000804b = 0;
  if (DAT_20008050 != '\0') {
    DAT_20008061 = 0;
    DAT_20008060 = 0;
    DAT_2000805e = 0;
    g_display_update = 0;
  }
  return;
}


// ==== Mode_SetFullScale @ 0800d5f4

/* g_full_scale = table 0x08025a3c[mode][range] (7 per mode); diode: 30000, or 15000 with
   g_diode_15v. From Mode_ApplyChange and Setup_DiodeVoltage. high */

void Mode_SetFullScale(void)

{
  if (g_mode == 0xb) {
    if (g_diode_15v == '\0') {
      g_full_scale = 30000;
    }
    else {
      g_full_scale = 15000;
    }
  }
  else {
    g_full_scale = *(undefined4 *)(&DAT_08025a3c + (uint)g_range * 4 + (uint)g_mode * 0x1c);
  }
  return;
}


// ==== VA_SubAlternate @ 0800d664

/* every 10 ms in VA modes (13-15, 22-24): after 300 ticks (3 s) toggles 0x200080c2 and 0800d6e0
   sets the sub-display decimal 0x20008040 for V or A (by range flags 0x200080c6/c7) and refreshes:
   the sub display alternates V / A every 3 s. medium */

undefined4 VA_SubAlternate(void)

{
  undefined4 unaff_r7;
  
  if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
      (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) &&
     (DAT_20007fd8 = DAT_20007fd8 + 1, 299 < DAT_20007fd8)) {
    DAT_200080c2 = DAT_200080c2 != '\x01';
    VA_SubSelectDecimals();
  }
  return unaff_r7;
}


// ==== VA_SubSelectDecimals @ 0800d6e0

/* VA sub display half switch (VA_SubAlternate, Setup_NextPage to page 10): decimals 0x20008040 for
   V (0801801e) or I (VA_SubDecimalsAmps) by 0x200080c2; clears 0x20007fd8, sub dirty flags
   0x20008062/63. high */

undefined4 VA_SubSelectDecimals(void)

{
  undefined4 unaff_r7;
  
  if (DAT_200080c2 == '\0') {
    VA_SubDecimalsVolts();
  }
  else {
    VA_SubDecimalsAmps();
  }
  DAT_20007fd8 = 0;
  DAT_20008063 = 1;
  DAT_20008062 = 1;
  return unaff_r7;
}


// ==== Setup_InitialPage @ 0800d718

/* g_setup_page start: factory mode -> 5 (year); ACV (no AC+DC), ACmV, AC/DC VA, AC/DC current -> 10
   (mode page; current also 0x200080aa = 100); else 0 (temp unit). high */

void Setup_InitialPage(void)

{
  if (g_factory_mode == '\0') {
    if ((g_mode == '\x02') && (DAT_200080ba == '\0')) {
      g_setup_page = 10;
    }
    else if (g_mode == '\x04') {
      g_setup_page = 10;
    }
    else if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
            (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
      g_setup_page = 10;
    }
    else if (((g_mode == '\x10') || (g_mode == '\x12')) ||
            ((g_mode == '\x14' || (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))
             ))) {
      g_setup_page = 10;
      DAT_200080aa = 100;
    }
    else {
      g_setup_page = 0;
    }
  }
  else {
    g_setup_page = 5;
  }
  return;
}


// ==== Mode_ConfigureHyCalCap @ 0800d80c

/* (reload) Mode_ConfigureHyCal(reload) then Cap_SetTimeout; from Mode_ApplyChange, VA_ApplyRange,
   DC/AC VA handlers. high */

void Mode_ConfigureHyCalCap(undefined1 param_1)

{
  Mode_ConfigureHyCal(param_1);
  Cap_SetTimeout();
  return;
}


// ==== Mode_ConfigureHyCal @ 0800d81e

/* (reload) HY_SetMode(g_mode, g_range); reload 1 -> Cal_LoadRecord, CurrentZero_Load,
   Cal_LoadRecord(6,0) too (counter clock cal) unless Hz, and for AC modes 2/4/16/18/20 and LowZ the
   AC frequency cal points (08011aa4). Via 0800d80c (+ Cap_SetTimeout). medium */

void Mode_ConfigureHyCal(char param_1)

{
  HY_SetMode(g_mode,g_range);
  if (param_1 == '\x01') {
    Cal_LoadRecord(g_mode,g_range);
    CurrentZero_Load(g_mode,g_range);
    if (g_mode != '\x06') {
      Cal_LoadRecord(6,0);
    }
  }
  if ((param_1 == '\x01') &&
     ((((g_mode == '\x10' || (g_mode == '\x12')) || (g_mode == '\x14')) ||
      ((g_mode == '\x02' || (g_mode == '\x04')))))) {
    Cal_LoadAcFreqPoints();
  }
  if ((param_1 == '\x01') && (g_mode == '\0')) {
    Cal_LoadAcFreqPoints();
  }
  return;
}


// ==== VA_ApplyRange @ 0800d8c2

/* VA re-range from VA_VoltsAutorange/VA_AmpsAutorange: flags 0x200080c3 = 1, 0x200080c5 = 0, TDM
   channel 0, 0800d80c(1) (HY + cal reload), VA_Reset, Frontend_SetCurrentB, Reading_SetSettleCount,
   08017700/080171fa (LCD). Never executed. medium */

undefined4 VA_ApplyRange(void)

{
  undefined4 unaff_r7;
  
  DAT_200080c3 = 1;
  DAT_200080c5 = 0;
  g_tdm_channel = 0;
  Mode_ConfigureHyCalCap(1);
  VA_Reset();
  Frontend_SetCurrentB(g_mode,g_range);
  Reading_SetSettleCount();
  LCD_DrawVaMilliIcon(g_mode,g_range);
  LCD_UpdateMainDP();
  return unaff_r7;
}


// ==== Display_DecimalPlaces @ 0800d920

/* (mode, range) -> digits after the decimal point, e.g. DCV r0..3 = 4,3,2,1
   (5.0000/50.000/500.00/600.0), Ohm 3,2,4,3,2,4,3, LowZ 1, continuity 2, diode 4 (3 with 15 V);
   mode 0x19 -> 1. [video "decimal point for (mode, range)", confirmed] high */

undefined1 Display_DecimalPlaces(byte param_1,byte param_2)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if (param_1 == 0x19) {
    return 1;
  }
  if (param_1 == 0) {
LAB_0800da0e:
    uVar1 = 1;
  }
  else {
    if ((param_1 != 2) && (1 < param_1)) {
      if ((param_1 == 4) || ((param_1 < 4 || (param_1 == 6)))) {
LAB_0800da82:
        if (param_2 == 0) {
          return 3;
        }
        if (param_2 == 2) {
          return 4;
        }
        if (param_2 < 2) {
          return 2;
        }
        if (param_2 == 4) {
          return 2;
        }
        if (param_2 < 4) {
          return 3;
        }
        if (param_2 == 6) {
          return 3;
        }
        if (param_2 < 6) {
          return 4;
        }
        return 0;
      }
      if ((param_1 < 6) || (param_1 == 8)) goto LAB_0800da0e;
      if (7 < param_1) {
        if (param_1 == 10) {
          return 2;
        }
        if (param_1 < 10) goto LAB_0800da82;
        if (param_1 == 0xc) {
          if (param_2 == 0) {
            return 3;
          }
          if (param_2 == 2) {
            return 4;
          }
          if (param_2 < 2) {
            return 2;
          }
          if (param_2 == 4) {
            return 2;
          }
          if (param_2 < 4) {
            return 3;
          }
          if (param_2 == 5) {
            return 0;
          }
          return 0;
        }
        if (param_1 < 0xc) {
          if (g_diode_15v == '\0') {
            return 4;
          }
          return 3;
        }
        if (param_1 == 0xe) {
LAB_0800d9ba:
          if (param_2 == 0) {
            return 3;
          }
          if (param_2 == 2) {
            return 2;
          }
          if (param_2 < 2) {
            return 2;
          }
          if (param_2 == 3) {
            return 1;
          }
          return 0;
        }
        if (param_1 < 0xe) {
LAB_0800d9e4:
          if (param_2 == 0) {
            return 2;
          }
          if (param_2 == 2) {
            return 1;
          }
          if (param_2 < 2) {
            return 1;
          }
          if (param_2 == 3) {
            return 0;
          }
          return 0;
        }
        if (param_1 == 0x10) goto LAB_0800da82;
        if (param_1 < 0x10) {
LAB_0800d990:
          if (param_2 == 0) {
            return 1;
          }
          if (param_2 == 2) {
            return 3;
          }
          if (param_2 < 2) {
            return 0;
          }
          if (param_2 == 3) {
            return 2;
          }
          return 0;
        }
        if (param_1 != 0x12) {
          if (param_1 < 0x12) goto LAB_0800da82;
          if (param_1 == 0x14) {
LAB_0800da30:
            if (param_2 == 0) {
              return 2;
            }
            if (param_2 == 2) {
              return 3;
            }
            if (param_2 < 2) {
              return 4;
            }
            return 0;
          }
          if (0x13 < param_1) {
            if (param_1 == 0x16) goto LAB_0800d9e4;
            if (0x15 < param_1) {
              if (param_1 != 0x18) {
                if (0x17 < param_1) {
                  return 0;
                }
                goto LAB_0800d9ba;
              }
              goto LAB_0800d990;
            }
            goto LAB_0800da30;
          }
        }
      }
    }
    if (param_2 == 0) {
      uVar1 = 4;
    }
    else if (param_2 == 2) {
      uVar1 = 2;
    }
    else if (param_2 < 2) {
      uVar1 = 3;
    }
    else if (param_2 == 4) {
      uVar1 = 2;
    }
    else if (param_2 < 4) {
      uVar1 = 1;
    }
  }
  return uVar1;
}


// ==== Reading_SetSettleCount @ 0800db48

/* 0x20000238 = display updates to discard after a mode/range change, per mode (ACV 3, ACmV 4,
   Hz/pulse/duty/continuity 0, Ohm 1-2, DCV 2, DCmV 1, LowZ DC 5, cap 1, current w/ burden 3/0, else
   3; AC+DC 1, PEAK 5). Autorange decrements it (sets 0x2000805c) and Display_Update skips 0800e848
   until 0. high */

void Reading_SetSettleCount(void)

{
  if ((((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
       ((g_mode == '\x11' || (g_mode == '\x13')))) || (g_mode == '\x15')) && (DAT_200080a5 != '\0'))
  {
    if (g_tdm_channel == '\0') {
      DAT_20000238 = 3;
    }
    else {
      DAT_20000238 = 0;
    }
  }
  else if (g_mode == '\x02') {
    DAT_20000238 = 3;
  }
  else if (g_mode == '\x04') {
    DAT_20000238 = 4;
  }
  else if (((g_mode == '\x06') || (g_mode == '\a')) || (g_mode == '\b')) {
    DAT_20000238 = 0;
  }
  else if (g_mode == '\t') {
    if (g_range < 6) {
      if (g_range == 5) {
        DAT_20000238 = 2;
      }
      else {
        DAT_20000238 = 1;
      }
    }
    else {
      DAT_20000238 = 2;
    }
  }
  else if (g_mode == '\n') {
    DAT_20000238 = 0;
  }
  else if (g_mode == '\x01') {
    DAT_20000238 = 2;
  }
  else if (g_mode == '\x03') {
    DAT_20000238 = 1;
  }
  else if ((g_mode == '\0') && (DAT_20008098 == '\0')) {
    DAT_20000238 = 5;
  }
  else if ((g_mode == '\f') && (g_range < 2)) {
    DAT_20000238 = 1;
  }
  else if (((g_mode == '\f') && (1 < g_range)) && (g_range < 5)) {
    DAT_20000238 = 1;
  }
  else if ((g_mode == '\f') && (g_range == 5)) {
    DAT_20000238 = 1;
  }
  else {
    DAT_20000238 = 3;
  }
  if ((g_mode == '\x02') && (DAT_200080ba != '\0')) {
    DAT_20000238 = 1;
  }
  if ((g_mode == '\x02') && (DAT_200080b7 != '\0')) {
    DAT_20000238 = 5;
  }
  return;
}


// ==== Display_Update @ 0800dcfc

/* autorange, then the display steps (to name). high */

undefined4 Display_Update(void)

{
  undefined4 unaff_r7;
  
  Autorange();
  if (DAT_20000238 == '\0') {
    Display_VoltageWarning();
  }
  Hold_AutoHoldUpdate();
  MinMax_Update();
  Rel_Apply();
  DAT_20008048 = DAT_20008046 != '\0';
  Display_LatchOverload();
  return unaff_r7;
}


// ==== Autorange @ 0800de24

/* up > FS + FS/10 (FS/12 w/ 0x200080f8), down < FS/10; not in MIN/MAX, manual, HOLD. high (traced +
   emulated) */

void Autorange(void)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  
  if ((g_mode == 0xc) && ((DAT_20008042 & DAT_20008046) != 0)) {
    DAT_20000238 = '\0';
    DAT_2000805c = 0;
  }
  else {
    if (((((g_mode != 6) && (g_mode != 7)) && (g_mode != 8)) &&
        (((g_mode != 0xd && (g_mode != 0xe)) &&
         ((g_mode != 0xf && ((g_mode != 0x16 && (g_mode != 0x17)))))))) && (g_mode != 0x18)) {
      DAT_20008046 = 0;
    }
    if (DAT_20000238 == '\0') {
      DAT_2000805c = 0;
      if (g_cal_mode == '\0') {
        if ((((g_mode != 0xd) && (g_mode != 0xe)) && (g_mode != 0xf)) &&
           (((g_mode != 0x16 && (g_mode != 0x17)) && (g_mode != 0x18)))) {
          if (((g_mode == 6) || (g_mode == 7)) || (g_mode == 8)) {
            DAT_20008045 = 0;
          }
          else {
            if (g_mode == 0xb) {
              uVar1 = g_full_scale;
            }
            else if (DAT_200080f8 == '\0') {
              uVar1 = g_full_scale / 10 + g_full_scale;
            }
            else {
              uVar1 = g_full_scale / 0xc + g_full_scale;
            }
            if ((g_mode == 0xc) && (g_range == 5)) {
              uVar2 = g_full_scale / 100;
            }
            else {
              uVar2 = g_full_scale / 10;
            }
            if (((g_mode == 0x14) || (g_mode == 0x15)) && (g_range == 2)) {
              uVar2 = uVar2 * 5;
            }
            if (-1 < (int)g_reading) {
              uVar4 = g_reading;
            }
            else {
              uVar4 = -g_reading;
            }
            DAT_20008045 = -1 >= (int)g_reading;
            if (((DAT_2000804e == '\0') && (DAT_20008042 == 0)) &&
               (1 < (byte)(&DAT_20000000)[g_mode])) {
              if (uVar1 < uVar4) {
                if (((DAT_2000804f == '\0') ||
                    (((DAT_20008050 != 0 && (g_mode != 6)) &&
                     ((g_mode != 7 && ((g_mode != 8 && (g_mode != 0xc)))))))) &&
                   ((int)(uint)g_range < (int)((byte)(&DAT_20000000)[g_mode] - 1))) {
                  iVar3 = Autorange_OhmNoHunt(1);
                  if (iVar3 == 0) {
                    g_range = g_range + 1;
                    DAT_2000804b = 1;
                    DAT_2000805c = 1;
                    if (DAT_20008050 != 0) {
                      DAT_20007da0 = 0;
                    }
                  }
                }
                else {
                  DAT_20008046 = 1;
                  if ((((DAT_20008050 & 1) != 0) && (g_mode == 9)) && (g_range == 6)) {
                    DAT_20008055 = 1;
                  }
                }
              }
              else if ((((DAT_20008050 & DAT_20008055 & 1) == 0) || (g_mode != 9)) || (g_range != 6)
                      ) {
                if (uVar4 < uVar2) {
                  if (((DAT_2000804f == '\0') && (g_range != 0)) &&
                     (iVar3 = Autorange_OhmNoHunt(0), iVar3 == 0)) {
                    if (((g_mode == 0xc) && (g_range == 2)) && (uVar4 < 0xb)) {
                      DAT_200080a2 = 1;
                    }
                    g_range = g_range - 1;
                    DAT_2000804b = 1;
                    DAT_2000805c = 1;
                    if (DAT_20008050 != 0) {
                      DAT_20007da0 = 0;
                    }
                  }
                }
                else {
                  DAT_20008035 = 0;
                }
              }
              else {
                DAT_20008055 = 0;
                g_range = DAT_20008056;
                DAT_2000804b = 1;
                DAT_2000805c = 1;
              }
              if (DAT_20008046 == 0) {
                DAT_200080f8 = '\0';
              }
              else {
                DAT_200080f8 = '\x01';
              }
            }
            else if (g_mode == 5) {
              if (DAT_200080ad == '\x01') {
                DAT_20008045 = 0;
                DAT_20008046 = 1;
              }
              else if (DAT_200080ad == '\x02') {
                DAT_20008045 = 1;
                DAT_20008046 = 1;
              }
            }
            else if (uVar1 < uVar4) {
              DAT_20008046 = 1;
            }
          }
        }
      }
      else {
        DAT_200080f8 = '\0';
      }
    }
    else {
      DAT_20000238 = DAT_20000238 + -1;
      DAT_200080f8 = '\0';
      DAT_2000805c = 1;
    }
  }
  return;
}


// ==== Display_LatchOverload @ 0800e278

/* Display_Update last step: 0x20008049 = OL flag 0x20008046 (the copy Record_EncodeMain uses). high
    */

void Display_LatchOverload(void)

{
  DAT_20008049 = DAT_20008046 != '\0';
  return;
}


// ==== Autorange_OhmNoHunt @ 0800e2a0

/* (up) Autorange veto, Ohm (mode 9) autoranging, not cal: an up request records 0x20008035 = 1; a
   down request right after an up returns 1 (blocked) until Autorange clears 0x20008035 with the
   reading in range; other modes clear it. 0x20008036 = 100 is written but never read. medium */

undefined4 Autorange_OhmNoHunt(char param_1)

{
  undefined4 uVar1;
  
  if ((DAT_20008042 == '\0' && g_cal_mode == '\0') && (g_mode == '\t')) {
    if ((param_1 == '\0') && (DAT_20008035 == '\x01')) {
      uVar1 = 1;
    }
    else {
      if (param_1 == '\0') {
        DAT_20008035 = '\x02';
      }
      else {
        DAT_20008035 = '\x01';
      }
      DAT_20008036 = 100;
      uVar1 = 0;
    }
  }
  else {
    DAT_20008035 = '\0';
    uVar1 = 0;
  }
  return uVar1;
}


// ==== Hold_AutoHoldUpdate @ 0800e30c

/* Display_Update step with HOLD/auto-hold (0x2000804f/0x20008050): only |reading| > 999 counts
   (Ohm/continuity: > 999 and not OL); new value held (0x20007da0, beep 0800eed0(50), log flag
   0x20007fe2) when two successive readings agree within +-20 and differ from the held one by > 400;
   otherwise freezes the display (g_display_update = 0). medium */

undefined4 Hold_AutoHoldUpdate(void)

{
  undefined4 in_r3;
  uint uVar1;
  
  if (((DAT_2000804f != '\0' || DAT_20008050 != '\0') && (DAT_2000805c == '\0')) &&
     ((g_mode != '\t' ||
      ((((DAT_20008042 != '\0' || (g_range == 0)) || (5 < g_range)) || (DAT_2000805a == '\0')))))) {
    if ((((g_mode == '\t') || (g_mode == '\n')) && ((999 < (int)g_reading && (DAT_20008046 == 0))))
       || (((g_mode != '\t' && (g_mode != '\n')) &&
           ((999 < (int)g_reading || ((int)g_reading < -999)))))) {
      if (DAT_20008052 != '\0') {
        DAT_20008052 = '\0';
        DAT_20008051 = '\0';
        DAT_20007fb2 = 0;
      }
      if (DAT_20008051 == '\0' && DAT_20008046 == 0) {
        g_display_update = 0;
      }
      else if (((g_mode == '\t') || (g_mode == '\n')) || (DAT_20008046 == 0)) {
        if (((g_mode == '\t') || (g_mode == '\n')) && (DAT_20008046 != 0)) {
          g_display_update = 0;
          DAT_20007da0 = 1000000;
          DAT_20008057 = '\0';
        }
        else if ((((DAT_20008054 & (DAT_20008046 ^ 1)) == 0) &&
                 ((int)g_reading < (int)(DAT_20007da0 + 400))) &&
                ((int)(DAT_20007da0 - 400) < (int)g_reading)) {
          g_display_update = 0;
          DAT_20008057 = '\0';
        }
        else {
          DAT_20008054 = 0;
          if ((int)g_reading < 0) {
            uVar1 = -g_reading;
          }
          else {
            uVar1 = g_reading;
          }
          if (uVar1 <= g_full_scale / 10 + g_full_scale) {
            if (DAT_20008057 == '\0') {
              DAT_20008057 = '\x01';
              DAT_20007da4 = g_reading;
            }
            else if (((int)g_reading < (int)(DAT_20007da4 - 0x14)) ||
                    ((int)(DAT_20007da4 + 0x14) < (int)g_reading)) {
              DAT_20008057 = '\0';
            }
            else {
              DAT_20008057 = '\0';
              DAT_20007dac = g_reading;
              DAT_20008053 = 1;
              DAT_20007da0 = g_reading;
              DAT_20008056 = g_range;
              DAT_20008055 = 0;
              if (DAT_20007fe1 != '\0') {
                DAT_20007fe2 = 1;
              }
              Buzzer_OnAlways(0x32);
            }
          }
        }
        DAT_20008051 = '\0';
        DAT_20007fb2 = 0;
      }
    }
    else {
      DAT_20008052 = '\x01';
      DAT_20008051 = '\0';
      DAT_20007fb2 = 0;
      DAT_20007da0 = g_reading;
      g_display_update = 0;
    }
  }
  return in_r3;
}


// ==== Rel_Apply @ 0800e594

/* Display_Update REL step (REL on 0x20008058, not HOLD 0x2000804f, not OL): on arming (0x20008059)
   reference 0x20007da8 = g_reading, or the MIN/MAX value 0x20007d9c when MIN/MAX is on; g_reading
   -= reference. high */

void Rel_Apply(void)

{
  if (((DAT_2000804f == '\0') && (DAT_20008058 != '\0')) && (DAT_20008046 == '\0')) {
    if (DAT_20008059 != '\0') {
      DAT_20008059 = '\0';
      if (DAT_2000804e == '\0') {
        DAT_20007da8 = g_reading;
      }
      else {
        DAT_20007da8 = DAT_20007d9c;
      }
    }
    g_reading = g_reading - DAT_20007da8;
  }
  return;
}


// ==== MinMax_Update @ 0800e604

/* Display_Update step, MIN/MAX on (0x2000804e) and not HOLD: max 0x20007d88, min 0x20007d8c,
   running mean 0x20007d90 (exact integer mean with remainder 0x20007d94, count 0x20007d98 capped at
   1e6); OL sets max/min to +-100000; g_reading = selected value for states 1..3. high */

void MinMax_Update(void)

{
  int iVar1;
  
  if ((DAT_2000804e != 0) && (DAT_2000804f == '\0')) {
    if (DAT_20008046 == '\0') {
      DAT_20007d9c = g_reading;
      if (DAT_20007d88 < g_reading) {
        DAT_20007d88 = g_reading;
      }
      if (g_reading < DAT_20007d8c) {
        DAT_20007d8c = g_reading;
      }
      if (DAT_20007d98 == 0) {
        DAT_20007d98 = 1;
        DAT_20007d90 = g_reading;
        DAT_20007d94 = 0;
      }
      else {
        DAT_20007d98 = DAT_20007d98 + 1;
        if (1000000 < DAT_20007d98) {
          DAT_20007d98 = 1000000;
        }
        iVar1 = (g_reading - DAT_20007d90) + DAT_20007d94;
        DAT_20007d90 = iVar1 / DAT_20007d98 + DAT_20007d90;
        DAT_20007d94 = iVar1 - DAT_20007d98 * (iVar1 / DAT_20007d98);
      }
    }
    else if (DAT_20008045 == '\0') {
      DAT_20007d88 = 100000;
      DAT_20007d9c = 100000;
    }
    else {
      DAT_20007d8c = -100000;
      DAT_20007d9c = -100000;
    }
    iVar1 = MinMax_Selected(0);
    if (DAT_2000804e < 4) {
      g_reading = iVar1;
    }
  }
  return;
}


// ==== MinMax_Selected @ 0800e758

/* (live) value of MIN/MAX state 0x2000804e: 1 MAX, 2 MIN, 3 AVG, 4 (arg 1) live 0x20007d6c; sets OL
   flag 0x20008046 / sign 0x20008045 at +-100000. high */

uint MinMax_Selected(uint param_1)

{
  if (DAT_2000804e == 1) {
    param_1 = DAT_20007d88;
    if ((int)DAT_20007d88 < 100000) {
      if ((int)DAT_20007d88 < 100000) {
        DAT_20008046 = 0;
      }
    }
    else {
      DAT_20008045 = 0;
      DAT_20008046 = 1;
    }
  }
  else if (DAT_2000804e != 0) {
    if (DAT_2000804e == 3) {
      param_1 = DAT_20007d90;
      if (DAT_20007d98 != 0) {
        DAT_20008046 = 0;
      }
    }
    else if (DAT_2000804e < 3) {
      param_1 = DAT_20007d8c;
      if ((int)DAT_20007d8c < -99999) {
        DAT_20008045 = 1;
        DAT_20008046 = 1;
      }
      else if ((int)DAT_20007d8c < 100000) {
        DAT_20008046 = 0;
      }
    }
    else if ((DAT_2000804e == 4) && ((param_1 & 0xff) != 0)) {
      param_1 = DAT_20007d6c;
      if ((int)DAT_20007d6c < -99999) {
        DAT_20008045 = 1;
        DAT_20008046 = 1;
      }
      else if ((int)DAT_20007d6c < 100000) {
        DAT_20008046 = 0;
      }
      else {
        DAT_20008045 = 0;
        DAT_20008046 = 1;
      }
    }
  }
  return param_1;
}


// ==== Display_VoltageWarning @ 0800e848

/* Display_Update step: 0x200080b1 = high-voltage icon (>= 30 V: DCV/ACV r1 30000, r2 3000, r3 300
   counts; LowZ 300; VA V-half >= 30000 in ranges 1/3; or OL) -> 0801a370 (LCD shadow 0x20007794 bit
   7); 0x200080b2 = >= 600 V or OL on the 600 V range/LowZ -> TIM2 beeps 500 ms every other 0.5 s.
   high */

undefined4 Display_VoltageWarning(void)

{
  undefined4 unaff_r7;
  
  DAT_200080b2 = 0;
  DAT_200080b1 = 0;
  if (((g_mode == '\x01') || (g_mode == '\x02')) && (g_range == 3)) {
    DAT_200080b2 = Value_AtLeastAbs(g_reading,6000);
    if (DAT_20008046 != '\0') {
      DAT_200080b2 = 1;
    }
  }
  else if ((g_mode == '\0') &&
          (DAT_200080b2 = Value_AtLeastAbs(g_reading,6000), DAT_20008046 != '\0')) {
    DAT_200080b2 = 1;
  }
  if ((g_mode == '\x01') || (g_mode == '\x02')) {
    if (g_range != 0) {
      if (g_range == 2) {
        DAT_200080b1 = Value_AtLeastAbs(g_reading,3000);
      }
      else if (g_range < 2) {
        DAT_200080b1 = Value_AtLeastAbs(g_reading,30000);
      }
      else if (g_range == 3) {
        DAT_200080b1 = Value_AtLeastAbs(g_reading,300);
      }
    }
    if ((g_range != 0) && (DAT_20008046 != '\0')) {
      DAT_200080b1 = 1;
    }
  }
  else if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
           ((g_mode == '\x16' || (g_mode == '\x17')))) || (g_mode == '\x18')) {
    if (((g_range == 1) || (g_range == 3)) &&
       (DAT_200080b1 = Value_AtLeastAbs(g_va_volts,30000), DAT_200080ca != '\0')) {
      DAT_200080b1 = 1;
    }
  }
  else if ((g_mode == '\0') &&
          (DAT_200080b1 = Value_AtLeastAbs(g_reading,300), DAT_20008046 != '\0')) {
    DAT_200080b1 = 1;
  }
  LCD_DrawHvIcon();
  return unaff_r7;
}


// ==== Value_AtLeastAbs @ 0800ea24

/* (value, limit) 1 when value >= limit or value <= -limit; Display_VoltageWarning thresholds
   (300/3000/30000/6000 counts). high */

undefined1 Value_AtLeastAbs(int param_1,int param_2)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if ((param_2 <= param_1) || (param_1 <= -param_2)) {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== BootScreen @ 0800ea3a

/* 2 s "V-x.xx" boot screen and its key checks (MODE+REL cal, MEM SD cal, RANGE all-segments, SETUP
   factory). high (Oct 31) */

undefined4 BootScreen(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  Frontend_PwrCtl2On(0,0);
  g_diode_15v = 0;
  Frontend_SetLedEnb(0);
  Frontend_SetVA(0);
  Frontend_SetTemp(0);
  Frontend_SetFre(0);
  Frontend_SetCurrentA(0,0);
  Frontend_SetCurrentB(0,0);
  LCD_DrawVersion();
  LCD_Flush();
  g_ms_countdown = 2000;
  HY_SetMode(5,0);
  while( true ) {
    while( true ) {
      while( true ) {
        do {
          if ((g_ms_countdown == 0) && (g_boot_sd_state != '\x04')) goto LAB_0800ebe0;
        } while (g_tick10ms == '\0');
        g_tick10ms = '\0';
        Keys_Process();
        if (g_boot_action != '\0') break;
        if (g_boot_sd_state != '\0') {
          if (g_boot_sd_state == '\x01') {
            LCD_MainSdCal();
          }
          else if (g_boot_sd_state == '\x02') {
            if (DAT_20008088 == '\0') {
              if (DAT_20008089 != '\0') {
                LCD_MainSdSet();
              }
            }
            else {
              LCD_MainSdSave();
            }
          }
          else if (g_boot_sd_state == '\x03') {
            if (DAT_20008088 == '\0') {
              if (DAT_20008089 != '\0') {
                iVar1 = CalFile_Load();
                if (iVar1 == 0) {
                  Show_SAvEd();
                  g_boot_sd_state = '\x04';
                }
                else {
                  Show_Err();
                  g_boot_sd_state = '\x04';
                }
              }
            }
            else {
              iVar1 = CalFile_Save();
              if (iVar1 == 0) {
                Show_SAvEd();
                g_boot_sd_state = '\x04';
              }
              else {
                Show_Err();
                g_boot_sd_state = '\x04';
              }
            }
          }
        }
      }
      if (g_boot_action != '\x01') break;
      LCD_MainCal1();
      LCD_SubCal();
      LCD_Flush();
    }
    if (g_boot_action == '\x02') break;
    if (g_boot_action == '\x03') {
      g_factory_mode = 1;
      DAT_20008062 = 1;
      DAT_200080dd = DAT_200080d7;
LAB_0800ebe0:
      g_boot_sd_state = 0;
      g_boot_action = 0;
      g_ms_countdown = 0;
      DAT_20008089 = 0;
      DAT_20008088 = 0;
      LCD_ClearShadow();
      LCD_Flush();
      return unaff_r7;
    }
  }
  g_cal_mode = 1;
  DAT_20008062 = 1;
  CurrentZero_ClearAll();
  goto LAB_0800ebe0;
}


// ==== Cap_SetTimeout @ 0800ec28

/* capacitance (mode 12): measurement timeout 0x20007fc4 in ticks by range (mostly 200, some 300,
   150 for ranges 0-1 on PF8-strap-low meters, 300 with range lock 0x20008042), clears 0x20007fc6
   and the timeout flag 0x200080a3 (set when the tick countdown ends). medium */

void Cap_SetTimeout(void)

{
  uint uVar1;
  
  if (g_mode == '\f') {
    DAT_200080a3 = 0;
    DAT_20007fc6 = 0;
    if (g_pf8_strap_low == '\0') {
      if (g_range < 2) {
        if (DAT_20008042 == '\0') {
          DAT_20007fc4 = 200;
        }
        else {
          DAT_20007fc4 = 300;
        }
      }
      else {
        uVar1 = g_range - 2;
        if (uVar1 < 3) {
          DAT_20007fc4 = 200;
        }
        else if (uVar1 == 3) {
          DAT_20007fc4 = 300;
        }
      }
    }
    else if (g_range == 0) {
      DAT_20007fc4 = 200;
    }
    else {
      if (g_range != 2) {
        if (g_range < 2) {
          if (DAT_20008042 == '\0') {
            DAT_20007fc4 = 0x96;
            DAT_20007fc6 = 0;
            DAT_200080a3 = 0;
            return;
          }
          DAT_20007fc4 = 300;
          DAT_20007fc6 = 0;
          DAT_200080a3 = 0;
          return;
        }
        if ((g_range != 4) && (3 < g_range)) {
          if (g_range == 5) {
            DAT_20007fc4 = 300;
            DAT_20007fc6 = 0;
            DAT_200080a3 = 0;
            return;
          }
          DAT_20007fc6 = 0;
          DAT_200080a3 = 0;
          return;
        }
      }
      DAT_20007fc4 = 200;
    }
  }
  return;
}


// ==== Delay_Loop @ 0800ecf6

/* bare busy loop (timing scales with the CPU clock). high */

void Delay_Loop(uint param_1)

{
  uint uVar1;
  
  for (uVar1 = 0; uVar1 < param_1; uVar1 = uVar1 + 1) {
  }
  return;
}


// ==== SysTick_Work @ 0800ed0c

/* decrements 0x20007d10, which nothing sets: SysTick unused by 2.05. high */

void SysTick_Work(void)

{
  if (DAT_20007d10 != 0) {
    DAT_20007d10 = DAT_20007d10 + -1;
  }
  return;
}


// ==== APO_Reset @ 0800ed26

/* APO countdown 1800 s (3600 s in MIN/MAX). high */

void APO_Reset(void)

{
  if (DAT_2000804e == '\0') {
    g_apo_seconds = 0x708;
  }
  else {
    g_apo_seconds = 0xe10;
  }
  DAT_200080e8 = 0;
  DAT_200080e9 = 0;
  DAT_200080e6 = 0;
  return;
}


// ==== Counter_AcGateAlternate @ 0800ed64

/* every 10 ms in ACV/ACmV/AC current (no burden, no AC+DC) and LowZ-AC: after 100 ticks (1 s; LowZ
   2000 = 20 s) toggles 0x2000809b and the counter preset 0x2000001c 0x8000/0xE000 (Hz sub-display
   gate, 0x20007e38 = 0x1000000 - preset*256) and re-sends HY_SetMode. medium */

void Counter_AcGateAlternate(void)

{
  ushort uVar1;
  
  if ((DAT_200080a5 == '\0') &&
     ((((((g_mode == '\x10' || (g_mode == '\x12')) || (g_mode == '\x14')) ||
        ((g_mode == '\x02' || (g_mode == '\x04')))) && (DAT_200080ba == '\0')) ||
      ((g_mode == '\0' && (DAT_20008098 != '\0')))))) {
    if (g_mode == '\0') {
      uVar1 = 2000;
    }
    else {
      uVar1 = 100;
    }
    DAT_20007fc2 = DAT_20007fc2 + 1;
    if (uVar1 < DAT_20007fc2) {
      DAT_20007fc2 = 0;
      DAT_2000809b = DAT_2000809b != '\x01';
      if ((bool)DAT_2000809b) {
        DAT_2000001c = 0x8000;
      }
      else {
        DAT_2000001c = 0xe000;
      }
      HY_SetMode(g_mode,g_range);
    }
  }
  return;
}


// ==== Beep_Short @ 0800ee50

/* 1 x 100 ms: key accepted. high */

undefined4 Beep_Short(void)

{
  undefined4 unaff_r7;
  
  Beep(1,1,100);
  return unaff_r7;
}


// ==== Beep_Long @ 0800ee5e

/* 1 x 200 ms. high */

undefined4 Beep_Long(void)

{
  undefined4 unaff_r7;
  
  Beep(1,1,200);
  return unaff_r7;
}


// ==== Beep_Double @ 0800ee6c

/* 3 toggles x 50 ms = two pulses: key not available / diode jump. high */

undefined4 Beep_Double(void)

{
  undefined4 unaff_r7;
  
  Beep(1,3,0x32);
  return unaff_r7;
}


// ==== Beep @ 0800ee7a

/* (reset_apo, toggles, ms): TIM3 drives PE7; every beep resets APO. high */

void Beep(char param_1,undefined1 param_2,undefined2 param_3)

{
  if (param_1 == '\x01') {
    APO_Reset();
  }
  DAT_20007fb6 = param_3;
  DAT_2000807a = param_2;
  Buzzer_On(param_3);
  return;
}


// ==== Buzzer_On @ 0800eea8  [GPIOE]

/* (ms) buzzer active 0x20008079, duration 0x20007fb4, PE7 high only if g_buzzer_enabled; from Beep
   and Mode_ApplyChange(300). high */

void Buzzer_On(undefined2 param_1)

{
  DAT_20008079 = 1;
  DAT_20007fb4 = param_1;
  if (g_buzzer_enabled != '\0') {
    GPIOE.BSRR._0_2_ = 0x80;
  }
  return;
}


// ==== Buzzer_OnAlways @ 0800eed0  [GPIOE]

/* (ms) as Buzzer_On but ignores g_buzzer_enabled: continuity, diode, auto-hold capture, HV warning
   (TIM2) and main beeps sound even with the buzzer set off. high */

void Buzzer_OnAlways(undefined2 param_1)

{
  DAT_20008079 = 1;
  DAT_20007fb4 = param_1;
  GPIOE.BSRR._0_2_ = 0x80;
  return;
}


// ==== Cal_Offset @ 0800eef0

/* not in diode mode: Cal_SubtractOffset, or Cal_OffsetAccumulate 0800ef44 while an offset capture
   is armed (0x2000808c). high */

undefined8 Cal_Offset(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  if (g_mode != '\v') {
    if (DAT_2000808c == '\0') {
      param_1 = Cal_SubtractOffset(param_1);
    }
    else {
      param_1 = Cal_OffsetAccumulate(param_1);
    }
  }
  return CONCAT44(param_4,param_1);
}


// ==== Cal_OffsetAccumulate @ 0800ef44

/* (sample) from Cal_Offset: offset capture over 0x20008095 = 16 samples (sum 0x20007e1c; Ohm r5
   drops the first 8), then g_cal_offset = sum / 16 (Ohm r5 /8, r6 /4: 4x the mean, possible bug),
   Cal_StoreField(0), done 0x2000808d. high */

undefined4 Cal_OffsetAccumulate(int param_1)

{
  int iVar1;
  
  iVar1 = 0x10;
  if (DAT_20008095 == 0x10) {
    DAT_20007e1c = param_1;
  }
  else if ((g_mode == '\t') && (g_range == '\x05')) {
    if (DAT_20008095 < 9) {
      DAT_20007e1c = param_1 + DAT_20007e1c;
    }
    else {
      DAT_20007e1c = 0;
    }
  }
  else {
    DAT_20007e1c = param_1 + DAT_20007e1c;
  }
  DAT_20008095 = DAT_20008095 - 1;
  if (DAT_20008095 == '\0') {
    DAT_2000808c = 0;
    DAT_2000808d = 1;
    g_display_update = 1;
    if ((g_mode == '\t') && (g_range == '\x05')) {
      iVar1 = 8;
    }
    else if ((g_mode == '\t') && (g_range == '\x06')) {
      iVar1 = 4;
    }
    g_cal_offset = DAT_20007e1c / iVar1;
    Cal_StoreField(0,g_cal_offset);
  }
  return 0;
}


// ==== Cal_CapOffsetCapture @ 0800f034

/* (sample) from Mode_Cap_OnCounter: 64-bit sum (0x20007c38) over 16 samples, then mean ->
   0x20007dfc and returned (to 0x20007d08), offset done 0x2000808d; returns 0 until then. medium */

undefined4 Cal_CapOffsetCapture(uint param_1)

{
  undefined4 uVar1;
  bool bVar2;
  
  bVar2 = CARRY4(DAT_20007c38,param_1);
  DAT_20007c38 = DAT_20007c38 + param_1;
  DAT_20007c3c = DAT_20007c3c + (uint)bVar2;
  DAT_20008095 = DAT_20008095 + -1;
  if (DAT_20008095 == '\0') {
    DAT_2000808c = 0;
    DAT_2000808d = 1;
    g_display_update = 1;
    uVar1 = __aeabi_ldivmod(DAT_20007c38,DAT_20007c3c,0x10,0);
    DAT_20007dfc = uVar1;
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}


// ==== Cal_AcOffsetCapture @ 0800f0ac

/* calibration mode, SETUP: 16-block average -> 0x20007e04, then 'offset done' 0x2000808d. high
   (emulated) */

int Cal_AcOffsetCapture(int param_1)

{
  int iVar1;
  
  iVar1 = 0;
  if (DAT_20008095 == '\x10') {
    DAT_20007e1c = param_1;
  }
  else {
    DAT_20007e1c = param_1 + DAT_20007e1c;
  }
  DAT_20008095 = DAT_20008095 + -1;
  if (DAT_20008095 == '\0') {
    DAT_2000808c = 0;
    DAT_2000808d = 1;
    iVar1 = DAT_20007e1c / 0x10;
    DAT_20007e04 = iVar1;
  }
  g_display_update = 1;
  return iVar1;
}


// ==== Cal_AcFreqWindow @ 0800f13c

/* AC modes: reference frequency window (0.1 Hz units) per range; sets the per-point flags
   0x2000809c.. . high */

undefined1 Cal_AcFreqWindow(char param_1)

{
  int iVar1;
  undefined1 uVar2;
  
  uVar2 = 0;
  if ((((g_mode != '\x10') && (g_mode != '\x12')) && (g_mode != '\x14')) &&
     ((g_mode != '\x02' && (g_mode != '\x04')))) {
    return 0;
  }
  iVar1 = Freq_ToDeciHz(DAT_2000803e,DAT_20007d84);
  if (((g_mode == '\x02') && (1 < g_range)) && ((7999 < iVar1 && (iVar1 < 0x2ee1)))) {
    if (param_1 == '\0') {
      DAT_2000809c = 1;
    }
    else {
      DAT_2000809d = 1;
    }
    uVar2 = 1;
    DAT_20000238 = 5;
  }
  else if (((g_mode == '\x02') || (g_mode == '\x04')) && (89999 < iVar1)) {
    if (param_1 == '\0') {
      DAT_2000809c = 1;
    }
    else {
      DAT_2000809d = 1;
    }
    uVar2 = 1;
    DAT_20000238 = 5;
  }
  else if (((g_mode == '\x02') || (g_mode == '\x04')) && ((17999 < iVar1 && (iVar1 < 0x55f1)))) {
    if (g_mode == '\x04') {
      return 2;
    }
    if (param_1 == '\0') {
      DAT_2000809e = 1;
    }
    else {
      DAT_2000809f = 1;
    }
    uVar2 = 1;
    DAT_20000238 = 5;
  }
  else if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
          ((7999 < iVar1 && (iVar1 < 0x2ee1)))) {
    if (g_mode == '\x14') {
      return 2;
    }
    if ((g_mode == '\x10') && (g_range == 0)) {
      return 2;
    }
    if ((g_mode == '\x12') && (g_range == 0)) {
      return 2;
    }
    if (param_1 == '\0') {
      DAT_2000809c = 1;
    }
    else {
      DAT_2000809d = 1;
    }
    uVar2 = 1;
    DAT_20000238 = 5;
  }
  else if (param_1 == '\0') {
    Cal_AcFreqClear();
  }
  return uVar2;
}


// ==== Cal_SubtractOffset @ 0800f2fc

/* value - 0x20007e00 (VA: 0x20007e24 / 0x20007e28). high */

int Cal_SubtractOffset(int param_1)

{
  if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
     (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
    if (g_tdm_channel == '\0') {
      param_1 = param_1 - DAT_20007e24;
    }
    else {
      param_1 = param_1 - DAT_20007e28;
    }
  }
  else {
    param_1 = param_1 - g_cal_offset;
  }
  return param_1;
}


// ==== Cal_SubtractOffsetAC @ 0800f368

/* medium */

int Cal_SubtractOffsetAC(int param_1)

{
  param_1 = param_1 - g_cal_offset;
  if (param_1 < 1) {
    param_1 = 0;
  }
  return param_1;
}


// ==== Cal_Gain @ 0800f37e

/* Cal_DivideGain; during a gain capture Cal_DcGainCapture 0800f470 (was misread as a VA variant).
   high */

undefined8 Cal_Gain(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  
  if (DAT_20008090 == '\0') {
    if (DAT_2000808e == '\0') {
      uVar1 = Cal_DivideGain(param_1);
    }
    else {
      uVar1 = Cal_DcGainCapture(param_1);
    }
  }
  else {
    uVar1 = Cal_DcGainCapture(param_1);
  }
  return CONCAT44(param_4,uVar1);
}


// ==== Cal_DivideGain @ 0800f3b8

/* trunc(value / gain + 0.5); gain 0 -> 0; ACV/ACmV with 1 kHz LPF: gain / 10. high */

undefined4 Cal_DivideGain(undefined4 param_1)

{
  undefined4 uVar1;
  undefined4 uVar2;
  char cVar3;
  undefined8 uVar4;
  
  uVar1 = Cal_GetGain();
  cVar3 = '\x01';
  __aeabi_cfcmpeq(uVar1);
  if (cVar3 == '\0') {
    if (((g_mode == '\x02') || (g_mode == '\x04')) && (g_lpf_1khz == '\x01')) {
      uVar2 = __aeabi_i2f(param_1);
      uVar1 = __aeabi_fdiv(uVar1,0x41200000);
      uVar1 = __aeabi_fdiv(uVar2,uVar1);
    }
    else {
      uVar2 = __aeabi_i2f(param_1);
      uVar1 = __aeabi_fdiv(uVar2,uVar1);
    }
  }
  else {
    uVar1 = 0;
  }
  uVar4 = __aeabi_f2d(uVar1);
  __aeabi_dadd((int)uVar4,(int)((ulonglong)uVar4 >> 0x20),0,0x3fe00000);
  uVar1 = __aeabi_d2iz();
  return uVar1;
}


// ==== Cal_DcGainCapture @ 0800f470  [DAT]

/* (sample) Cal_Gain path while gain capture 0x2000808e (or Ohm r6 trim 0x20008090): 64-bit sum over
   0x20008095 samples (Ohm r5/r6 and the trim drop the first 8), /16 (Ohm r5 /8, r6 /4) via
   __aeabi_ldivmod -> 0x20007e20, Cal_ComputeGain, store gain (Cal_StoreField 1); Ohm r6 trim =
   avg/gain - 30000 -> EEPROM 0x178; in progress returns sample x 60.80732 (display). medium */

undefined8 Cal_DcGainCapture(uint param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  bool bVar3;
  undefined8 uVar4;
  
  uVar2 = 0x10;
  iVar1 = (int)param_1 >> 0x1f;
  if (DAT_20008090 == '\0') {
    if ((g_mode == '\t') && (4 < g_range)) {
      if (DAT_20008095 < 9) {
        bVar3 = CARRY4(DAT_20007c38,param_1);
        DAT_20007c38 = DAT_20007c38 + param_1;
        DAT_20007c3c = DAT_20007c3c + iVar1 + (uint)bVar3;
      }
      else {
        DAT_20007c38 = 0;
        DAT_20007c3c = 0;
      }
    }
    else {
      bVar3 = CARRY4(DAT_20007c38,param_1);
      DAT_20007c38 = DAT_20007c38 + param_1;
      DAT_20007c3c = DAT_20007c3c + iVar1 + (uint)bVar3;
    }
    DAT_20008095 = DAT_20008095 - 1;
    if (DAT_20008095 == '\0') {
      DAT_2000808e = 0;
      DAT_2000808f = 1;
      g_display_update = 1;
      if ((g_mode == '\t') && (g_range == 5)) {
        uVar2 = 8;
      }
      else if ((g_mode == '\t') && (g_range == 6)) {
        uVar2 = 4;
      }
      __aeabi_ldivmod(DAT_20007c38,DAT_20007c3c,uVar2,0);
      DAT_20007e20 = __aeabi_i2f();
      Cal_ComputeGain(0);
      uVar2 = __aeabi_f2iz(g_cal_gain);
      Cal_StoreField(1,uVar2);
      uVar2 = __aeabi_i2f(param_1);
      __aeabi_fdiv(uVar2,g_cal_gain);
      uVar2 = __aeabi_f2iz();
    }
    else {
      __aeabi_i2f(param_1);
      uVar4 = __aeabi_f2d();
      __aeabi_ddiv((int)uVar4,(int)((ulonglong)uVar4 >> 0x20),&DAT_4302b40f,0x404e6756);
      uVar2 = __aeabi_d2iz();
    }
  }
  else {
    if (DAT_20008095 < 9) {
      bVar3 = CARRY4(DAT_20007c38,param_1);
      DAT_20007c38 = DAT_20007c38 + param_1;
      DAT_20007c3c = DAT_20007c3c + iVar1 + (uint)bVar3;
    }
    else {
      DAT_20007c38 = 0;
      DAT_20007c3c = 0;
    }
    DAT_20008095 = DAT_20008095 - 1;
    if (DAT_20008095 == '\0') {
      DAT_20008090 = '\0';
      uVar2 = __aeabi_ldivmod(DAT_20007c38,DAT_20007c3c,8,0);
      uVar2 = __aeabi_i2f(uVar2);
      __aeabi_fdiv(uVar2,g_cal_gain);
      iVar1 = __aeabi_f2iz();
      DAT_20007e14 = iVar1 + -30000;
      Cal_StoreField(0x96,DAT_20007e14);
      DAT_20000238 = 5;
    }
    uVar2 = 0;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== Cal_GetGain @ 0800f648

/* 0x20007e10 (VA: 0x20007e2c / 0x20007e30). high */

undefined4 Cal_GetGain(void)

{
  undefined4 uVar1;
  
  if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
     (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
    if (g_tdm_channel == '\0') {
      uVar1 = DAT_20007e2c;
    }
    else {
      uVar1 = DAT_20007e30;
    }
  }
  else {
    uVar1 = g_cal_gain;
  }
  return uVar1;
}


// ==== Cal_CapGainCapture @ 0800f6c0

/* (sample, ref) from Mode_Cap_OnCounter, gain capture: 16-sample 64-bit mean; g_cal_offset = ref -
   (mean - ref)/9 (unsigned divide), Cal_StoreField(0); mode 12 gain = (0x20007e68 - offset)/10000
   else Cal_ComputeGain; Cal_StoreField(1, (int)gain). Never executed. low */

void Cal_CapGainCapture(uint param_1,int param_2)

{
  int iVar1;
  undefined4 uVar2;
  bool bVar3;
  
  bVar3 = CARRY4(DAT_20007c38,param_1);
  DAT_20007c38 = DAT_20007c38 + param_1;
  DAT_20007c3c = DAT_20007c3c + (uint)bVar3;
  DAT_20008095 = DAT_20008095 + -1;
  if (DAT_20008095 == '\0') {
    DAT_2000808e = 0;
    DAT_2000808f = 1;
    g_display_update = 1;
    iVar1 = __aeabi_ldivmod(DAT_20007c38,DAT_20007c3c,0x10,0);
    g_cal_offset = param_2 - (uint)(iVar1 - param_2) / 9;
    Cal_StoreField(0,g_cal_offset);
    if (g_mode == '\f') {
      uVar2 = __aeabi_ui2f(DAT_20007e68 - g_cal_offset);
      g_cal_gain = __aeabi_fdiv(uVar2,0x461c4000);
    }
    else {
      DAT_20007e20 = __aeabi_ui2f(iVar1 - g_cal_offset);
      Cal_ComputeGain(0);
    }
    uVar2 = __aeabi_f2iz(g_cal_gain);
    Cal_StoreField(1,uVar2);
  }
  return;
}


// ==== Cal_AcGainCapture @ 0800f7b8

/* calibration mode, MEM: 16-block average, two-point fit (10% / 100%), writes the record; writes
   the 0xA5 variant markers. high (emulated) */

void Cal_AcGainCapture(int param_1,int param_2)

{
  int iVar1;
  undefined4 uVar2;
  
  if (DAT_20008095 == '\x10') {
    DAT_20007e1c = param_1;
  }
  else {
    DAT_20007e1c = param_1 + DAT_20007e1c;
  }
  DAT_20008095 = DAT_20008095 + -1;
  if (DAT_20008095 == '\0') {
    DAT_2000808e = 0;
    DAT_2000808f = 1;
    iVar1 = DAT_20007e1c / 0x10;
    g_cal_offset = param_2 - (iVar1 - param_2) / 9;
    Cal_StoreField(0,g_cal_offset);
    if (g_mode == '\f') {
      uVar2 = __aeabi_i2f(DAT_20007e68 - g_cal_offset);
      g_cal_gain = __aeabi_fdiv(uVar2,0x461c4000);
    }
    else {
      DAT_20007e20 = __aeabi_i2f(iVar1 - g_cal_offset);
      Cal_ComputeGain(0);
    }
    uVar2 = __aeabi_f2iz(g_cal_gain);
    Cal_StoreField(1,uVar2);
    if ((g_mode == '\x02') && (g_range == '\0')) {
      Settings_ClearVariantAcv0();
    }
    if ((g_mode == '\t') && (g_range == '\x06')) {
      Settings_ClearVariantOhm6();
      g_variant_ohm6 = 1;
    }
  }
  g_display_update = 1;
  return;
}


// ==== Cal_AcFreqPointStore @ 0800f8e4

/* (point 1..4, reading) AC freq-response cal: error vs nominal (points 1/3 at 10% = 5000, 2/4 at
   100% = 50000; ACV r3 600/6000) -> 0x20007e4c/50/54/58, then 08011e88 writes it s16 to EEPROM;
   points flagged by Cal_AcFreqWindow (0x2000809c..f). medium */

undefined4 Cal_AcFreqPointStore(byte param_1,int param_2,undefined4 param_3,undefined4 param_4)

{
  if (param_1 == 1) {
    if ((g_mode == '\x02') && (g_range == '\x03')) {
      DAT_20007e4c = param_2 + -600;
    }
    else {
      DAT_20007e4c = param_2 + -5000;
    }
  }
  else if (param_1 != 0) {
    if (param_1 == 3) {
      if ((g_mode == '\x02') && (g_range == '\x03')) {
        DAT_20007e54 = param_2 + -600;
      }
      else {
        DAT_20007e54 = param_2 + -5000;
      }
    }
    else if (param_1 < 3) {
      if ((g_mode == '\x02') && (g_range == '\x03')) {
        DAT_20007e50 = param_2 + -6000;
      }
      else {
        DAT_20007e50 = param_2 + -50000;
      }
    }
    else if (param_1 == 4) {
      if ((g_mode == '\x02') && (g_range == '\x03')) {
        DAT_20007e58 = param_2 + -6000;
      }
      else {
        DAT_20007e58 = param_2 + -50000;
      }
    }
  }
  Cal_StoreAcFreqPoints(g_mode,g_range,param_1);
  return param_4;
}


// ==== Cal_ComputeGain @ 0800f9f0

/* (va_current) g_cal_gain = captured average 0x20007e20 / reference counts for (mode, range): 50000
   default, 30000 for range 2 of DCV/ACV/ACmV and diode, 6000 range 3 (600 V), 10000 A range 2 (and
   VA current r>=2), cap 10000 (r5 1000), PEAK 42426.4 (30000 x sqrt 2). high */

void Cal_ComputeGain(char param_1)

{
  uint uVar1;
  
  uVar1 = (uint)g_mode;
  if (uVar1 != 1) {
    if (uVar1 != 2) {
      if (uVar1 == 3) goto LAB_0800fa9e;
      if (uVar1 != 4) {
        if (uVar1 != 5) {
          if (uVar1 == 9) {
            if (g_range == 6) {
              Cal_GainFrom50000(0);
              return;
            }
            Cal_GainFrom50000(0);
            return;
          }
          if (uVar1 != 10) {
            if (uVar1 == 0xb) {
              Cal_GainFrom30000(0);
              return;
            }
            if (uVar1 == 0xc) {
              if (g_range == 5) {
                Cal_GainFromRef(0,0x447a0000);
                return;
              }
              Cal_GainFromRef(0,0x461c4000);
              return;
            }
            if (uVar1 == 0xd) {
LAB_0800fa44:
              Cal_GainFrom50000(0);
              return;
            }
            if (uVar1 == 0xe) {
LAB_0800fa4c:
              Cal_GainFrom50000(0);
              return;
            }
            if (uVar1 != 0xf) {
              if (uVar1 - 0x10 < 4) goto LAB_0800fac0;
              if (uVar1 - 0x14 < 2) {
                if (g_range == 2) {
                  Cal_GainFrom10000(0);
                  return;
                }
                Cal_GainFrom50000(0);
                return;
              }
              if (uVar1 == 0x16) goto LAB_0800fa44;
              if (uVar1 == 0x17) goto LAB_0800fa4c;
              if (uVar1 != 0x18) {
                return;
              }
            }
            if (param_1 == '\0') {
              Cal_GainFrom50000(0);
              return;
            }
            if (g_range < 2) {
              Cal_GainFrom50000(0);
              return;
            }
            Cal_GainFrom10000(0);
            return;
          }
        }
LAB_0800fac0:
        Cal_GainFrom50000(0);
        return;
      }
    }
    if (DAT_200080b7 != '\0') {
      Cal_GainFromPeak();
      return;
    }
  }
  if (g_range == 2) {
    Cal_GainFrom30000(0);
    return;
  }
LAB_0800fa9e:
  if (g_range == 3) {
    Cal_GainFrom6000(0);
  }
  else {
    Cal_GainFrom50000(0);
  }
  return;
}


// ==== Cal_GainFromRef @ 0800fb1e

/* (unused, ref float) g_cal_gain = captured average 0x20007e20 / ref; capacitance 10000.0 (r5
   1000.0). Never executed. high */

undefined4
Cal_GainFromRef(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  g_cal_gain = __aeabi_fdiv(DAT_20007e20,param_2);
  return param_4;
}


// ==== Cal_GainFrom50000 @ 0800fb38

/* g_cal_gain = 0x20007e20 / 50000.0, the default reference in Cal_ComputeGain. high */

void Cal_GainFrom50000(void)

{
  g_cal_gain = __aeabi_fdiv(DAT_20007e20,0x47435000);
  return;
}


// ==== Cal_GainFrom30000 @ 0800fb58

/* g_cal_gain = 0x20007e20 / 30000.0 (range 2 of DCV/ACV/ACmV, diode). high */

void Cal_GainFrom30000(void)

{
  g_cal_gain = __aeabi_fdiv(DAT_20007e20,0x46ea6000);
  return;
}


// ==== Cal_GainFrom10000 @ 0800fb78

/* g_cal_gain = 0x20007e20 / 10000.0 (A range 2, VA current r >= 2). high */

void Cal_GainFrom10000(void)

{
  g_cal_gain = __aeabi_fdiv(DAT_20007e20,0x461c4000);
  return;
}


// ==== Cal_GainFrom6000 @ 0800fb98

/* g_cal_gain = 0x20007e20 / 6000.0 (range 3, 600 V). high */

void Cal_GainFrom6000(void)

{
  g_cal_gain = __aeabi_fdiv(DAT_20007e20,0x45bb8000);
  return;
}


// ==== Cal_GainFromPeak @ 0800fbb8

/* g_cal_gain = 0x20007e20 / 42426.4068 (30000 x sqrt 2, double divide) in 1 ms PEAK. high */

undefined4 Cal_GainFromPeak(void)

{
  undefined4 unaff_r7;
  undefined8 uVar1;
  
  uVar1 = __aeabi_f2d(DAT_20007e20);
  __aeabi_ddiv((int)uVar1,(int)((ulonglong)uVar1 >> 0x20),0x4816f01,0x40e4b74d);
  g_cal_gain = __aeabi_d2f();
  return unaff_r7;
}


// ==== Frontend_SetAll @ 0800fbe8

/* (mode, range) front-end switch state for a mode: Frontend_PwrCtl2On(mode, 0x20008098),
   SetSwitches, SetLedEnb, SetFre, SetVA, SetTemp, SetCurrentA/B; also clears g_diode_15v. high */

undefined4
Frontend_SetAll(undefined1 param_1,undefined1 param_2,undefined4 param_3,undefined4 param_4)

{
  Frontend_PwrCtl2On(param_1,DAT_20008098);
  Frontend_SetSwitches(param_1);
  g_diode_15v = 0;
  Frontend_SetLedEnb(param_1);
  Frontend_SetFre(param_1);
  Frontend_SetVA(param_1);
  Frontend_SetTemp(param_1);
  Frontend_SetCurrentA(param_1,param_2);
  Frontend_SetCurrentB(param_1,param_2);
  return param_4;
}


// ==== Frontend_SetSwitches @ 0800fc60  [GPIOC GPIOD GPIOF]

/* analog front-end switches for a mode: CTL_A PD0, CTL_D PF4, CTL_E PF5, CTL_B PC11, DCmV PD7 (1
   kHz LPF case on ACV/ACmV); 100 = none (power-on default). high */

void Frontend_SetSwitches(char param_1)

{
  if ((((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) ||
     ((((param_1 == '\x02' || (param_1 == '\x04')) ||
       ((param_1 == '\0' || ((param_1 == '\r' || (param_1 == '\x0e')))))) || (param_1 == '\x0f'))))
  {
    if (((param_1 == '\x02') || (param_1 == '\x04')) && (g_lpf_1khz == '\x01')) {
      GPIOD.BSRR._2_2_ = 1;
      GPIOF.BSRR._2_2_ = 0x10;
      GPIOF.BSRR._0_2_ = 0x20;
    }
    else {
      GPIOD.BSRR._2_2_ = 1;
      GPIOF.BSRR._0_2_ = 0x10;
      GPIOF.BSRR._2_2_ = 0x20;
    }
  }
  else {
    GPIOD.BSRR._0_2_ = 1;
    GPIOF.BSRR._0_2_ = 0x10;
    GPIOF.BSRR._0_2_ = 0x20;
  }
  if (param_1 == '\t') {
    GPIOC.BSRR._2_2_ = 0x800;
  }
  else {
    GPIOC.BSRR._0_2_ = 0x800;
  }
  if (param_1 == '\x02') {
    GPIOD.BSRR._2_2_ = 0x80;
  }
  else {
    GPIOD.BSRR._0_2_ = 0x80;
  }
  return;
}


// ==== Frontend_SetLedEnb @ 0800fd30  [GPIOE GPIOF]

/* PE15 LED_CK / PF6 ENB per mode (diode, ACmV cases). high */

void Frontend_SetLedEnb(char param_1)

{
  if ((param_1 == '\v') && (g_diode_15v != '\0')) {
    GPIOE.BSRR._2_2_ = 0x8000;
    GPIOF.BSRR._0_2_ = 0x40;
  }
  else {
    if (param_1 == '\x04') {
      GPIOE.BSRR._2_2_ = 0x8000;
    }
    else {
      GPIOE.BSRR._0_2_ = 0x8000;
    }
    GPIOF.BSRR._2_2_ = 0x40;
  }
  return;
}


// ==== Frontend_SetVA @ 0800fd80  [GPIOA]

/* PA11 VA_CTL per mode (14/15 VA). high */

void Frontend_SetVA(char param_1)

{
  if ((param_1 == '\x0e') || (param_1 == '\x0f')) {
    GPIOA.BSRR._2_2_ = 0x800;
  }
  else {
    GPIOA.BSRR._0_2_ = 0x800;
  }
  return;
}


// ==== Frontend_SetTemp @ 0800fdb0  [GPIOH]

/* PH2 TEMP_CTL: on in mode 5 (temperature). high */

void Frontend_SetTemp(char param_1)

{
  if (param_1 == '\x05') {
    GPIOH.BSRR._0_2_ = 4;
  }
  else {
    GPIOH.BSRR._2_2_ = 4;
  }
  return;
}


// ==== Frontend_SetFre @ 0800fdd0  [GPIOA]

/* PA12 FRE_CTL per mode (6-8 Hz/pulse/duty). high */

void Frontend_SetFre(char param_1)

{
  if (((param_1 == '\x06') || (param_1 == '\a')) || (param_1 == '\b')) {
    GPIOA.BSRR._2_2_ = 0x1000;
  }
  else {
    GPIOA.BSRR._0_2_ = 0x1000;
  }
  return;
}


// ==== Frontend_PwrCtl2On @ 0800fe00  [GPIOD]

/* PD1 HW_PWR_CTL2 high. high */

void Frontend_PwrCtl2On(void)

{
  GPIOD.BSRR._0_2_ = 2;
  return;
}


// ==== Frontend_SetCurrentA @ 0800fe14  [GPIOE]

/* current-mode switch (10..21, 13..15 VA); pins to confirm. medium */

void Frontend_SetCurrentA(char param_1)

{
  if (((((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) ||
      ((((param_1 == '\x11' || (param_1 == '\x13')) ||
        ((param_1 == '\x15' || ((param_1 == '\r' || (param_1 == '\x0e')))))) || (param_1 == '\x0f'))
      )) || (((param_1 == '\x16' || (param_1 == '\x17')) || (param_1 == '\x18')))) {
    GPIOE.BSRR._0_2_ = 0x1000;
  }
  else {
    GPIOE.BSRR._2_2_ = 0x1000;
  }
  return;
}


// ==== Frontend_SetCurrentB @ 0800fe74  [GPIOE]

/* PE5 AMP_CTL in current modes (16..21). medium */

void Frontend_SetCurrentB(char param_1,char param_2)

{
  if ((((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) ||
     (((param_1 == '\x11' || (param_1 == '\x13')) || (param_1 == '\x15')))) {
    if (param_2 == '\0') {
      GPIOE.BSRR._0_2_ = 0x20;
    }
    else {
      GPIOE.BSRR._2_2_ = 0x20;
    }
  }
  else if (((param_1 == '\r') || (param_1 == '\x0e')) ||
          ((param_1 == '\x0f' ||
           (((param_1 == '\x16' || (param_1 == '\x17')) || (param_1 == '\x18')))))) {
    if (DAT_200080c7 == '\0') {
      GPIOE.BSRR._0_2_ = 0x20;
    }
    else {
      GPIOE.BSRR._2_2_ = 0x20;
    }
  }
  else {
    GPIOE.BSRR._0_2_ = 0x20;
  }
  return;
}


// ==== Settings_LoadClock @ 0800ff18

/* date/time from the RTC chip (BCD) with defaults (year 16, month 7, ...). high (reader) */

void Settings_LoadClock(void)

{
  byte bVar1;
  
  bVar1 = RTC_ReadReg(2);
  if ((bVar1 == 0) || (0x99 < bVar1)) {
    DAT_200080d7 = 0x10;
  }
  else {
    DAT_200080d7 = BCD_ToBin(bVar1);
    if ((DAT_200080d7 == 0) || (99 < DAT_200080d7)) {
      DAT_200080d7 = 0x10;
    }
  }
  Delay_Loop(5);
  bVar1 = RTC_ReadReg(4);
  if ((bVar1 == 0) || (0x12 < bVar1)) {
    DAT_200080d8 = 7;
  }
  else {
    DAT_200080d8 = BCD_ToBin(bVar1);
    if ((DAT_200080d8 == 0) || (0xc < DAT_200080d8)) {
      DAT_200080d8 = 7;
    }
  }
  Delay_Loop(5);
  bVar1 = RTC_ReadReg(6);
  if ((bVar1 == 0) || (0x31 < bVar1)) {
    DAT_200080d9 = 1;
  }
  else {
    DAT_200080d9 = BCD_ToBin(bVar1);
    if ((DAT_200080d9 == 0) || (0x1f < DAT_200080d9)) {
      DAT_200080d9 = 1;
    }
  }
  Delay_Loop(5);
  bVar1 = RTC_ReadReg(10);
  if (bVar1 < 0x24) {
    DAT_200080da = BCD_ToBin(bVar1);
    if (0x17 < DAT_200080da) {
      DAT_200080da = 0xc;
    }
  }
  else {
    DAT_200080da = 0xc;
  }
  Delay_Loop(5);
  bVar1 = RTC_ReadReg(0xc);
  if (bVar1 < 0x5a) {
    DAT_200080db = BCD_ToBin(bVar1);
    if (0x3b < DAT_200080db) {
      DAT_200080db = 0;
    }
  }
  else {
    DAT_200080db = 0;
  }
  Delay_Loop(5);
  bVar1 = RTC_ReadReg(0xe);
  if (bVar1 < 0x5a) {
    DAT_200080dc = BCD_ToBin(bVar1);
    if (0x3b < DAT_200080dc) {
      DAT_200080dc = 0;
    }
  }
  else {
    DAT_200080dc = 0;
  }
  return;
}


// ==== RTC_PollSeconds @ 080100c4

/* every 10 ms: RTC register 0x0E (seconds) changed -> new-second flag 0x200080d4 and toggles
   0x200080e7 (1 Hz blink phase). medium */

void RTC_PollSeconds(void)

{
  char cVar1;
  
  cVar1 = RTC_ReadReg(0xe);
  if (DAT_2000023d != cVar1) {
    DAT_200080d4 = 1;
    DAT_2000023d = cVar1;
    if (DAT_200080e7 == '\x01') {
      DAT_200080e7 = '\0';
    }
    else {
      DAT_200080e7 = '\x01';
    }
  }
  return;
}


// ==== BCD_ToBin @ 08010118

/* high */

char BCD_ToBin(byte param_1)

{
  return (param_1 >> 4) * '\n' + (param_1 & 0xf);
}


// ==== Bcd_FromByte @ 0801013e

/* (0..99) -> packed BCD (tens << 4 | ones). high */

uint Bcd_FromByte(byte param_1)

{
  return (uint)param_1 % 10 + (param_1 / 10) * 0x10 & 0xff;
}


// ==== RTC_WriteClock @ 0801016c

/* writes the clock copy to the RTC: registers 2/4/6/10/12/14 = 0x200080d7..dc (year, month, day,
   hour, minute, second) via 080101ac. From BLE_ProcessRx (0xF8 set-time) and
   Setup_Year/MonthDay/HourMinute. high */

undefined4 RTC_WriteClock(void)

{
  undefined4 unaff_r7;
  
  RTC_WriteRegBcd(2,DAT_200080d7);
  RTC_WriteRegBcd(4,DAT_200080d8);
  RTC_WriteRegBcd(6,DAT_200080d9);
  RTC_WriteRegBcd(10,DAT_200080da);
  RTC_WriteRegBcd(0xc,DAT_200080db);
  RTC_WriteRegBcd(0xe,DAT_200080dc);
  return unaff_r7;
}


// ==== RTC_WriteRegBcd @ 080101ac

/* (reg, value) RTC_Select, command reg+1, value as BCD, RTC_Deselect; RTC_WriteClock for regs
   2/4/6/10/12/14 (year..second). high */

void RTC_WriteRegBcd(char param_1,undefined1 param_2)

{
  undefined1 uVar1;
  
  uVar1 = Bcd_FromByte(param_2);
  RTC_Select();
  Delay_Loop(5);
  RTC_ShiftOutNibble(param_1 + '\x01');
  RTC_WriteByte(uVar1);
  Delay_Loop(5);
  RTC_Deselect();
  return;
}


// ==== RTC_ReadReg @ 080101e2

/* NJU6350 RTC chip register read (CE, address, byte, release). high */

ulonglong RTC_ReadReg(undefined1 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  
  RTC_Select();
  RTC_ShiftOutNibble(param_1);
  uVar1 = RTC_ReadByte();
  RTC_Deselect();
  return CONCAT44(param_4,uVar1) & 0xffffffff000000ff;
}


// ==== RTC_ShiftOutNibble @ 08010204  [GPIOB]

/* (nibble) 4 bits LSB first to the RTC: data PB7, clock PB6 pulses with Delay_Loop(5); the register
   address (write: addr + 1). From 080101ac and RTC_ReadReg. high */

void RTC_ShiftOutNibble(uint param_1)

{
  uint uVar1;
  
  for (uVar1 = 0; uVar1 = uVar1 & 0xff, uVar1 < 4; uVar1 = uVar1 + 1) {
    param_1 = param_1 & 0xff;
    if (((int)param_1 >> uVar1 & 1U) == 1) {
      GPIOB.BSRR._0_2_ = 0x80;
    }
    else {
      GPIOB.BSRR._2_2_ = 0x80;
    }
    GPIOB.BSRR._0_2_ = 0x40;
    Delay_Loop(5);
    GPIOB.BSRR._2_2_ = 0x40;
    Delay_Loop(5);
  }
  return;
}


// ==== RTC_ReadByte @ 08010254  [GPIOB]

/* bit-banged RTC chip read: PB7 (data) to input (08010378), 8 clocks on PB6 (high, sample PB7,
   low), LSB first, PB7 back to output (0801033c); returns the byte. Used by RTC_ReadReg. high */

undefined8 RTC_ReadByte(void)

{
  ushort uVar1;
  undefined4 in_r3;
  byte bVar2;
  uint uVar3;
  
  uVar3 = 0;
  Delay_Loop(5);
  RTC_DataInput();
  Delay_Loop(5);
  for (bVar2 = 0; bVar2 < 8; bVar2 = bVar2 + 1) {
    GPIOB.BSRR._0_2_ = 0x40;
    Delay_Loop(5);
    uVar1 = GPIOB.IDR._0_2_;
    if ((int)((uint)uVar1 << 0x18) < 0) {
      uVar3 = uVar3 | 0x80;
    }
    GPIOB.BSRR._2_2_ = 0x40;
    Delay_Loop(5);
    if (bVar2 < 7) {
      uVar3 = uVar3 >> 1;
    }
  }
  RTC_DataOutput();
  Delay_Loop(5);
  return CONCAT44(in_r3,uVar3);
}


// ==== RTC_WriteByte @ 080102b8  [GPIOB]

/* bit-banged RTC chip write: 8 bits LSB first, PB6 clock low, data on PB7, clock high (data latched
   on the rising edge). Used by the register write 080101ac. high */

void RTC_WriteByte(uint param_1)

{
  uint uVar1;
  
  for (uVar1 = 0; uVar1 = uVar1 & 0xff, uVar1 < 8; uVar1 = uVar1 + 1) {
    GPIOB.BSRR._2_2_ = 0x40;
    Delay_Loop(5);
    param_1 = param_1 & 0xff;
    if (((int)param_1 >> uVar1 & 1U) == 1) {
      GPIOB.BSRR._0_2_ = 0x80;
    }
    else {
      GPIOB.BSRR._2_2_ = 0x80;
    }
    GPIOB.BSRR._0_2_ = 0x40;
    Delay_Loop(5);
  }
  return;
}


// ==== RTC_Select @ 08010308  [GPIOB GPIOC]

/* RTC 3-wire start: PB6, PB7 low, PC13 (CE) high. medium */

void RTC_Select(void)

{
  GPIOB.BSRR._2_2_ = 0x40;
  GPIOB.BSRR._2_2_ = 0x80;
  GPIOC.BSRR._0_2_ = 0x2000;
  return;
}


// ==== RTC_Deselect @ 0801031e  [GPIOB GPIOC]

/* RTC 3-wire end: PB6, PB7 low, PC13 (CE) low. medium */

void RTC_Deselect(void)

{
  GPIOB.BSRR._2_2_ = 0x40;
  GPIOB.BSRR._2_2_ = 0x80;
  GPIOC.BSRR._2_2_ = 0x2000;
  return;
}


// ==== RTC_DataOutput @ 0801033c  [GPIOB]

/* PB7 (RTC data) as push-pull output (GPIO_Init mode 1). high */

undefined8 RTC_DataOutput(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 local_a;
  undefined1 local_9;
  
  local_10 = 0x80;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOB,&local_10);
  return CONCAT17(local_9,CONCAT16(local_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== RTC_DataInput @ 08010378  [GPIOB]

/* PB7 (RTC data) as input with pull-up (GPIO_Init mode 0, pupd 1). high */

undefined8 RTC_DataInput(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 uStack_a;
  undefined1 local_9;
  
  local_10 = 0x80;
  local_c = 0;
  local_9 = 1;
  local_b = 3;
  GPIO_Init(&GPIOB,&local_10);
  return CONCAT17(local_9,CONCAT16(uStack_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== Date_ClampDay @ 080103a8

/* (year, month, day) day limited to Date_DaysInMonth; SETUP year and month/day edits. high */

byte Date_ClampDay(undefined1 param_1,undefined1 param_2,byte param_3)

{
  byte bVar1;
  
  bVar1 = Date_DaysInMonth(param_1,param_2);
  if (bVar1 < param_3) {
    param_3 = bVar1;
  }
  return param_3;
}


// ==== Date_DaysInMonth @ 080103d8

/* (year 0..255 = 2000+y, month 1..12) -> days (31,28/29,31,30,...); Gregorian leap rule (/4, not
   /100, /400); month 0 reads an uninitialized stack byte. Setup_MonthDay and day clamp 080103a8.
   high (emulated: 2024/2 29, 2023/2 28, 2100/2 28, 2025/4 30) */

undefined8 Date_DaysInMonth(byte param_1,uint param_2)

{
  uint uVar1;
  byte bStack_19;
  undefined4 local_18;
  undefined4 uStack_14;
  undefined4 uStack_10;
  
  local_18 = 0x1e1f1c1f;
  uStack_14 = 0x1f1f1e1f;
  uStack_10 = 0x1f1e1f1e;
  uVar1 = param_1 + 2000;
  if ((((uVar1 & 3) == 0) && (uVar1 % 100 != 0)) || (uVar1 % 400 == 0)) {
    local_18 = 0x1e1f1d1f;
  }
  return CONCAT44(local_18,(uint)(&bStack_19)[param_2 & 0xff]);
}


// ==== Settings_VariantAcv0 @ 080104bc

/* EEPROM 0x1F != 0xA5: board-variant flag for ACV lowest range. medium */

bool Settings_VariantAcv0(void)

{
  int iVar1;
  
  iVar1 = EE_ReadU8(0x1f);
  return iVar1 != 0xa5;
}


// ==== Settings_ClearVariantAcv0 @ 080104d4

/* Cal_AcGainCapture ACV r0: EEPROM 0x1F = 0xA5, verified, up to 10 tries (clears the boot-time
   g_variant_acv0, but RAM g_variant_acv0 is not updated until reboot). high */

void Settings_ClearVariantAcv0(void)

{
  int iVar1;
  byte bVar2;
  
  bVar2 = 0;
  while( true ) {
    if (9 < bVar2) {
      return;
    }
    EE_WriteByte(0x1f,0xa5);
    iVar1 = EE_ReadU8(0x1f);
    if (iVar1 == 0xa5) break;
    bVar2 = bVar2 + 1;
  }
  return;
}


// ==== CalFile_EeRead @ 080104f8

/* (offset) wrapper of EE_ReadU8 for CalFile_Save/CalFile_Load. high */

void CalFile_EeRead(undefined4 param_1)

{
  EE_ReadU8(param_1);
  return;
}


// ==== CalFile_EeWrite @ 08010504

/* (offset, byte) wrapper of EE_WriteByte for CalFile_Load. high */

undefined4
CalFile_EeWrite(undefined4 param_1,undefined1 param_2,undefined4 param_3,undefined4 param_4)

{
  EE_WriteByte(param_1,param_2);
  return param_4;
}


// ==== Record_Snapshot @ 08010516

/* main every 100 ms (0x200080d1, after Settings_LoadClock): current reading as a record: code
   0x20007fee = mode + range*32, flags 0x20007fef (b0 LowZ AC, b1 0x20008099, b2 LPF, b3 PEAK, b4
   AC+DC, b5 deg F), value via Record_EncodeMain; sub (Hz code range*32+6 or burden 0x19,
   0x20007d2c) and third (VA current mode code, 0x20007d30), b7 = OL. high */

void Record_Snapshot(void)

{
  DAT_20007fee = g_mode + g_range * ' ';
  DAT_20007fef = DAT_20008098 != '\0';
  if (DAT_20008099 != '\0') {
    DAT_20007fef = DAT_20007fef | 2;
  }
  if (g_lpf_1khz != '\0') {
    DAT_20007fef = DAT_20007fef | 4;
  }
  if (DAT_200080b7 != '\0') {
    DAT_20007fef = DAT_20007fef | 8;
  }
  if (DAT_200080ba != '\0') {
    DAT_20007fef = DAT_20007fef | 0x10;
  }
  if (g_temp_unit != '\0') {
    DAT_20007fef = DAT_20007fef | 0x20;
  }
  Record_EncodeMain();
  DAT_20007ff1 = 0;
  DAT_20007ff0 = '\0';
  DAT_20007d2c = 0;
  if (((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
      ((g_mode == '\x02' && (DAT_200080ba == '\0')))) || (g_mode == '\x04')) {
    if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) && (DAT_200080a5 != '\0')
       ) {
      DAT_20007ff0 = '\x19';
      if (DAT_200080a7 != '\0') {
        DAT_20007ff1 = 0x80;
      }
      DAT_20007d2c = DAT_20007e6c;
    }
    else {
      DAT_20007ff0 = DAT_2000803e * ' ' + '\x06';
      if (DAT_20008047 != '\0') {
        DAT_20007ff1 = 0x80;
      }
      DAT_20007d2c = DAT_20007d84;
    }
  }
  else if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
           ((g_mode == '\x16' || (g_mode == '\x17')))) || (g_mode == '\x18')) {
    if (((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) {
      DAT_20007ff0 = '\x02';
    }
    else {
      DAT_20007ff0 = '\x01';
    }
    if ((g_range == 1) || (g_range == 3)) {
      DAT_20007ff0 = DAT_20007ff0 + ' ';
    }
    if ((DAT_200080ca != '\0') || (DAT_200080cb != '\0')) {
      DAT_20007ff1 = 0x80;
    }
    DAT_20007d2c = DAT_20007eac;
  }
  DAT_20007ff3 = 0;
  DAT_20007ff2 = '\0';
  if ((((g_mode != '\r') && (g_mode != '\x0e')) && (g_mode != '\x0f')) &&
     (((g_mode != '\x16' && (g_mode != '\x17')) && (g_mode != '\x18')))) {
    DAT_20007d30 = 0;
    DAT_20007ff2 = 0;
    DAT_20007ff3 = 0;
    return;
  }
  if (g_mode == '\r') {
    DAT_20007ff2 = '\x10';
  }
  else if (g_mode == '\x0e') {
    DAT_20007ff2 = '\x12';
  }
  else if (g_mode == '\x0f') {
    DAT_20007ff2 = '\x14';
  }
  else if (g_mode == '\x16') {
    DAT_20007ff2 = '\x11';
  }
  else if (g_mode == '\x17') {
    DAT_20007ff2 = '\x13';
  }
  else if (g_mode == '\x18') {
    DAT_20007ff2 = '\x15';
  }
  if (g_mode == '\r') {
LAB_08010858:
    if (1 < g_range) {
      DAT_20007ff2 = DAT_20007ff2 + ' ';
    }
  }
  else {
    if (g_mode == '\x0e') {
LAB_08010876:
      if (1 < g_range) {
        DAT_20007ff2 = DAT_20007ff2 + ' ';
      }
      goto LAB_080108b0;
    }
    if (g_mode != '\x0f') {
      if (g_mode == '\x16') goto LAB_08010858;
      if (g_mode == '\x17') goto LAB_08010876;
      if (g_mode != '\x18') goto LAB_080108b0;
    }
    if (1 < g_range) {
      DAT_20007ff2 = DAT_20007ff2 + '@';
    }
  }
LAB_080108b0:
  if ((DAT_200080cb != '\0') || (DAT_200080ca != '\0')) {
    DAT_20007ff3 = 0x80;
  }
  if ((g_mode == '\r') || (g_mode == '\x16')) {
    DAT_20007d30 = DAT_20007eb4;
  }
  else {
    DAT_20007d30 = DAT_20007eb0;
  }
  return;
}


// ==== Record_EncodeMain @ 08010908

/* main value 0x20007d28 as magnitude + sign: ~x with flag 0x40 for negatives, OL -> 0x7FFFFFFF +
   0x80 (0xC0 negative OL, from 0x20008049); V/A modes take g_reading_fast (per-sample),
   temp/Hz/pulse/duty/Ohm/cap g_reading. high */

void Record_EncodeMain(void)

{
  byte bVar1;
  
  if (DAT_20008049 == '\0') {
    bVar1 = DAT_20007fef;
    if ((((g_mode < 6) || (8 < g_mode)) && (g_mode != 0xc)) && ((g_mode != 5 && (g_mode != 9)))) {
      if ((int)g_reading_fast < 0) {
        DAT_20007d28 = ~g_reading_fast;
        bVar1 = DAT_20007fef | 0x40;
      }
      else {
        DAT_20007d28 = g_reading_fast;
      }
    }
    else if ((int)g_reading < 0) {
      DAT_20007d28 = ~g_reading;
      bVar1 = DAT_20007fef | 0x40;
    }
    else {
      DAT_20007d28 = g_reading;
    }
  }
  else {
    DAT_20007d28 = 0x7fffffff;
    bVar1 = DAT_20007fef | 0x80;
    if (DAT_20008045 != '\0') {
      bVar1 = DAT_20007fef | 0xc0;
    }
  }
  DAT_20007fef = bVar1;
  return;
}


// ==== Log_Start @ 080109ea

/* WARNING: Removing unreachable block (ram,0x08010aa0) */
/* clears hold/rel/minmax, range locked (manual), log state reset, 0x2000801c = 1 (main opens the
   file). high (emulated) */

undefined4 Log_Start(void)

{
  undefined4 unaff_r7;
  
  if (DAT_2000803b == '\0') {
    g_setup_page = 100;
    if (DAT_20008050 == '\0') {
      DAT_20008043 = DAT_20008042;
      DAT_20008042 = 1;
      LCD_DrawAutoIcon(1);
    }
    DAT_20008058 = 0;
    DAT_2000804e = 0;
    DAT_2000804f = 0;
    DAT_20007fe3 = 1;
    DAT_20007fe2 = 0;
    DAT_20007fe4 = 0;
    DAT_20007f9c = 0;
    DAT_20007fa4 = 0;
    DAT_20007d38 = 0;
    DAT_20007d34 = 0;
    DAT_20008024 = 1;
    DAT_2000801c = 1;
    DAT_2000801d = 0;
    DAT_20007d48 = 0;
    DAT_20007d40 = 0;
    DAT_20008026 = 0;
    DAT_20008025 = 0;
    DAT_20007fae = 0;
    DAT_20007fa8 = g_log_interval + -1;
    DAT_20007ffa = 0;
    Log_SaveMode();
    Log_SaveStartTime();
  }
  else {
    DAT_2000801b = 0x14;
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== Log_Stop @ 08010af8

/* writes the MAX/MIN footer and closes. medium */

void Log_Stop(char param_1)

{
  Setup_InitialPage();
  Log_SaveCounters();
  Log_SaveStopTime();
  if ((DAT_20007fa4 != 0) && (DAT_20007fa4 < 0x3c)) {
    DAT_20007d44 = (uint)DAT_20007fa4;
  }
  DAT_20008025 = 1;
  DAT_20008024 = 0;
  if (param_1 == '\x01') {
    Log_WriteRetry();
  }
  Log_ViewInit();
  if (DAT_20008050 == '\0') {
    DAT_20008042 = DAT_20008043;
    LCD_DrawAutoIcon(1);
  }
  return;
}


// ==== Log_StoreSample @ 08010b64

/* main, when logging (0x20007fe1) and a sample is due (0x20007fe2, cleared): stores the current
   record into ring slot 0x20007fa4 (Record_Store; slot reset to 0 when > 59), slot++; after 60 rows
   sets 0x2000801c (main writes them to SD) and 0x20007d44 = 60; total count 0x20007d38++ (shown on
   sub page 100); sub redraw 0x20008062. high */

undefined4 Log_StoreSample(void)

{
  undefined4 unaff_r7;
  
  DAT_20007fe2 = 0;
  if (0x3b < DAT_20007fa4) {
    DAT_20007fa4 = 0;
  }
  Log_StoreRecord(DAT_20007fa4);
  DAT_20007fa4 = DAT_20007fa4 + 1;
  if (0x3b < DAT_20007fa4) {
    DAT_2000801c = 1;
    DAT_20007d44 = 0x3c;
  }
  DAT_20007d38 = DAT_20007d38 + 1;
  DAT_20008062 = 1;
  return unaff_r7;
}


// ==== Log_SaveCounters @ 08010bca

/* Log_Stop: ring slot 0x20007fa4 -> 0x20007fac and row count 0x20007d38 -> 0x20007d3c (restored by
   Log_RestoreCounters). high */

undefined4 Log_SaveCounters(void)

{
  DAT_20007fac = DAT_20007fa4;
  DAT_20007d3c = DAT_20007d38;
  return 0x4b0;
}


// ==== Log_RestoreCounters @ 08010be8

/* App_StateInit: ring slot 0x20007fa4 and total count 0x20007d38 from the copies Log_Stop saved
   (0x20007fac/0x20007d3c, 08010bca); both zeroed unless slot < 61 and count < 600000 (garbage
   guard); then Log_ViewInit. high */

void Log_RestoreCounters(void)

{
  DAT_20007fa4 = DAT_20007fac;
  DAT_20007d38 = DAT_20007d3c;
  if (DAT_20007fac < 0x3d) {
    if (DAT_20007d3c < 600000) goto LAB_08010c2e;
  }
  DAT_20007fa4 = 0;
  DAT_20007d38 = 0;
LAB_08010c2e:
  Log_ViewInit();
  return;
}


// ==== Log_ViewInit @ 08010c34

/* recall-view window over the 60-row ring: total 0x20007d38 <= 60 -> first 0x20007fa0 = 1, rows
   0x20007f9e = slot; else first = slot + 1, rows = 60; newest 0x20007fa2 = slot 0x20007fa4; view
   position 0x20007f9c = 1. From Log_Stop and Log_RestoreCounters. high */

void Log_ViewInit(void)

{
  if (DAT_20007d38 < 0x3d) {
    DAT_20007fa0 = 1;
    DAT_20007f9e = DAT_20007fa4;
  }
  else {
    DAT_20007fa0 = DAT_20007fa4 + 1;
    DAT_20007f9e = 0x3c;
  }
  DAT_20007fa2 = DAT_20007fa4;
  DAT_20007f9c = 1;
  return;
}


// ==== LogView_PosToSlot @ 08010c8c

/* (view position n, 1-based) -> ring slot number (1-based) first 0x20007fa0 + n - 1, wrapped once
   by the row count 0x20007f9e; LogView_Show passes slot-1 to Record_Load. Never executed. high */

short LogView_PosToSlot(ushort param_1)

{
  short sVar1;
  
  if ((int)(uint)DAT_20007f9e < (int)((uint)DAT_20007fa0 + (uint)param_1 + -1)) {
    sVar1 = (DAT_20007fa0 + param_1 + -1) - DAT_20007f9e;
  }
  else {
    sVar1 = DAT_20007fa0 + param_1 + -1;
  }
  return sVar1;
}


// ==== Log_SaveMode @ 08010ccc

/* Log_Start: g_mode/g_range -> 0x2000800d/0x2000800e (logging session's mode). high */

void Log_SaveMode(void)

{
  DAT_2000800d = g_mode;
  DAT_2000800e = g_range;
  return;
}


// ==== LogView_LoadMode @ 08010ce2

/* Key_ShortPress entering the recall view: view mode/range 0x20007feb/fec = logged mode
   0x2000800d/0e. Never executed. high */

void LogView_LoadMode(void)

{
  DAT_20007feb = DAT_2000800d;
  DAT_20007fec = DAT_2000800e;
  return;
}


// ==== Log_SaveStartTime @ 08010cfc

/* Log_Start: RTC date/time 0x200080d7..dc copied to the start-time fields 0x20007ffb..0x20008000
   and their backup 0x2000800f..14. high */

undefined4 Log_SaveStartTime(void)

{
  DAT_20007ffb = DAT_200080d7;
  DAT_20007ffc = DAT_200080d8;
  DAT_20007ffd = DAT_200080d9;
  DAT_20007ffe = DAT_200080da;
  DAT_20007fff = DAT_200080db;
  DAT_20008000 = DAT_200080dc;
  DAT_2000800f = DAT_200080d7;
  DAT_20008010 = DAT_200080d8;
  DAT_20008011 = DAT_200080d9;
  DAT_20008012 = DAT_200080da;
  DAT_20008013 = DAT_200080db;
  DAT_20008014 = DAT_200080dc;
  return 0x4c0;
}


// ==== Log_RestoreStartTime @ 08010d92

/* start-time fields 0x20007ffb..0x20008000 from the backup 0x2000800f..14; on entering the recall
   view (Key_ShortPress). Never executed. high */

void Log_RestoreStartTime(void)

{
  DAT_20007ffb = DAT_2000800f;
  DAT_20007ffc = DAT_20008010;
  DAT_20007ffd = DAT_20008011;
  DAT_20007ffe = DAT_20008012;
  DAT_20007fff = DAT_20008013;
  DAT_20008000 = DAT_20008014;
  return;
}


// ==== Log_SaveStopTime @ 08010de0

/* Log_Stop: RTC date/time 0x200080d7..dc copied to the stop-time fields 0x20008001..06 and their
   backup 0x20008015..1a. high */

undefined4 Log_SaveStopTime(void)

{
  DAT_20008001 = DAT_200080d7;
  DAT_20008002 = DAT_200080d8;
  DAT_20008003 = DAT_200080d9;
  DAT_20008004 = DAT_200080da;
  DAT_20008005 = DAT_200080db;
  DAT_20008006 = DAT_200080dc;
  DAT_20008015 = DAT_200080d7;
  DAT_20008016 = DAT_200080d8;
  DAT_20008017 = DAT_200080d9;
  DAT_20008018 = DAT_200080da;
  DAT_20008019 = DAT_200080db;
  DAT_2000801a = DAT_200080dc;
  return 0x4c6;
}


// ==== Log_RestoreStopTime @ 08010e98

/* stop-time fields 0x20008001..06 from the backup 0x20008015..1a; on entering the recall view.
   Never executed. high */

void Log_RestoreStopTime(void)

{
  DAT_20008001 = DAT_20008015;
  DAT_20008002 = DAT_20008016;
  DAT_20008003 = DAT_20008017;
  DAT_20008004 = DAT_20008018;
  DAT_20008005 = DAT_20008019;
  DAT_20008006 = DAT_2000801a;
  return;
}


// ==== Log_StoreRecord @ 08010ef4

/* (slot) wrapper of Record_Store; Log_StoreSample. high */

void Log_StoreRecord(undefined2 param_1)

{
  Record_Store(param_1);
  return;
}


// ==== Log_LoadRecord @ 08010f02

/* (index) wrapper of Record_Load; LogView_Show, Log_WriteDataRow. high */

void Log_LoadRecord(undefined2 param_1)

{
  Record_Load(param_1);
  return;
}


// ==== Settings_VariantOhm6 @ 08010f10

/* EEPROM 0x26 != 0xA5: board-variant flag for Ohm range 6. medium */

bool Settings_VariantOhm6(void)

{
  int iVar1;
  
  iVar1 = EE_ReadU8(0x26);
  return iVar1 != 0xa5;
}


// ==== Settings_ClearVariantOhm6 @ 08010f28

/* Cal_AcGainCapture Ohm r6: EEPROM 0x26 = 0xA5, verified, up to 10 tries; the caller then sets
   g_variant_ohm6 = 1, the opposite of what Settings_VariantOhm6 loads at the next boot (0). Never
   executed. high */

void Settings_ClearVariantOhm6(void)

{
  int iVar1;
  byte bVar2;
  
  bVar2 = 0;
  while( true ) {
    if (9 < bVar2) {
      return;
    }
    EE_WriteByte(0x26,0xa5);
    iVar1 = EE_ReadU8(0x26);
    if (iVar1 == 0xa5) break;
    bVar2 = bVar2 + 1;
  }
  return;
}


// ==== Settings_LoadDateStamp @ 08010f4c

/* EEPROM 0x01 YYMM, 0 -> 1708. high */

undefined4 Settings_LoadDateStamp(void)

{
  undefined4 unaff_r7;
  
  g_date_stamp = EE_ReadU16(1);
  if (g_date_stamp == 0) {
    g_date_stamp = 0x6ac;
  }
  return unaff_r7;
}


// ==== Settings_SaveDateStamp16 @ 08010f74

/* writes 0x01 from 0x20007f9a. high */

undefined4 Settings_SaveDateStamp16(void)

{
  undefined4 unaff_r7;
  
  EE_WriteU16(1,g_date_stamp);
  return unaff_r7;
}


// ==== Settings_LoadMeterId @ 08010f8c

/* EEPROM 0x03 u32, <= 99999 else 0. medium */

undefined4 Settings_LoadMeterId(void)

{
  undefined4 unaff_r7;
  
  g_meter_id = EE_ReadU32(3);
  if (99999 < g_meter_id) {
    g_meter_id = 0;
  }
  return unaff_r7;
}


// ==== Settings_SaveMeterId @ 08010fb8

/* high */

undefined4 Settings_SaveMeterId(void)

{
  undefined4 unaff_r7;
  
  EE_WriteU32(3,g_meter_id);
  return unaff_r7;
}


// ==== Mode_LastDcvAcv @ 08010ff0

/* Mode_FromRotary V position: EEPROM 0x08 -> 1 DCV / 2 ACV, 0x82 -> ACV + Mode_AcDcOn; else 1. high
    */

undefined8 Mode_LastDcvAcv(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 1;
  bVar1 = EE_ReadU8(8);
  if (bVar1 == 0x82) {
    uVar2 = 2;
    Mode_AcDcOn();
  }
  else if ((bVar1 == 1) || (bVar1 == 2)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastMv @ 08011020

/* Mode_FromRotary mV position: EEPROM 0x09 -> 3/4/5 (DCmV/ACmV/temp), else 3. high */

undefined8 Mode_LastMv(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 3;
  bVar1 = EE_ReadU8(9);
  if (((bVar1 == 3) || (bVar1 == 4)) || (bVar1 == 5)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastHz @ 08011046

/* Mode_FromRotary Hz position: EEPROM 0x0A -> 6/7/8 (Hz/pulse/duty), else 6. high */

undefined8 Mode_LastHz(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 6;
  bVar1 = EE_ReadU8(10);
  if (((bVar1 == 6) || (bVar1 == 7)) || (bVar1 == 8)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastOhm @ 0801106c

/* Mode_FromRotary Ohm position: EEPROM 0x0B -> 9..12 (Ohm/cont/diode/cap), else 9. high */

undefined8 Mode_LastOhm(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 9;
  bVar1 = EE_ReadU8(0xb);
  if ((((bVar1 == 9) || (bVar1 == 10)) || (bVar1 == 0xb)) || (bVar1 == 0xc)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastVa @ 08011098

/* Mode_FromRotary VA position (rotary 0x20): EEPROM 0x0C -> 24 or 15, else 24. high */

undefined8 Mode_LastVa(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 0x18;
  bVar1 = EE_ReadU8(0xc);
  if ((bVar1 == 0x18) || (bVar1 == 0xf)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastMicroAmp @ 080110b8

/* Mode_FromRotary uA position: EEPROM 0x0D -> 17/16/13/22, else 17. high */

undefined8 Mode_LastMicroAmp(void)

{
  byte bVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 0x11;
  bVar1 = EE_ReadU8(0xd);
  if ((((bVar1 == 0x11) || (bVar1 == 0x10)) || (bVar1 == 0xd)) || (bVar1 == 0x16)) {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(in_r3,uVar2);
}


// ==== Mode_LastMilliAmp @ 080110e4

/* Mode_FromRotary mA/A position: EEPROM 0x0E 19/21 -> 19, 18/20 -> 18, else 19 (Jack_Poll then
   picks mA or A). high */

undefined8 Mode_LastMilliAmp(void)

{
  char cVar1;
  undefined4 in_r3;
  byte bVar2;
  
  bVar2 = 0x13;
  cVar1 = EE_ReadU8(0xe);
  if ((cVar1 == '\x13') || (cVar1 == '\x15')) {
    bVar2 = 0x13;
  }
  else if ((cVar1 == '\x12') || (cVar1 == '\x14')) {
    bVar2 = 0x12;
  }
  return CONCAT44(in_r3,(uint)bVar2);
}


// ==== Mode_SaveLast @ 08011118

/* (EEPROM offset, mode) EE_WriteByte; offset 8 mode 2 with AC+DC 0x200080ba stores 0x82; from
   Mode_SaveForPosition. high */

undefined4 Mode_SaveLast(char param_1,char param_2,undefined4 param_3,undefined4 param_4)

{
  if (((param_1 == '\b') && (param_2 == '\x02')) && (DAT_200080ba != '\0')) {
    param_2 = -0x7e;
  }
  EE_WriteByte(param_1,param_2);
  return param_4;
}


// ==== Mode_SaveForPosition @ 08011154

/* Mode_Cycle: writes g_mode as the remembered mode of its dial position via 08011118 to EEPROM 0x08
   (DCV/ACV; 0x82 when AC+DC 0x200080ba), 0x09 (mV/temp), 0x0A (Hz/pulse/duty), 0x0B
   (Ohm/cont/diode/cap), 0x0C (VA 14/15/23/24: 0x18 if 23/24 else 0x0F), 0x0D (uA 13/16/17/22), 0x0E
   (mA/A 18..21). high */

void Mode_SaveForPosition(void)

{
  uint uVar1;
  undefined1 uVar2;
  
  uVar1 = (uint)g_mode;
  if (uVar1 - 1 < 2) {
    Mode_SaveLast(8,g_mode);
    return;
  }
  if (uVar1 - 3 < 3) {
    Mode_SaveLast(9,g_mode);
    return;
  }
  if (uVar1 - 6 < 3) {
    Mode_SaveLast(10,g_mode);
    return;
  }
  if (uVar1 - 9 < 4) {
    Mode_SaveLast(0xb,g_mode);
    return;
  }
  if (uVar1 != 0xd) {
    if (uVar1 - 0xe < 2) {
LAB_080111ce:
      if ((g_mode == 0x18) || (g_mode == 0x17)) {
        uVar2 = 0x18;
      }
      else {
        uVar2 = 0xf;
      }
      Mode_SaveLast(0xc,uVar2);
      return;
    }
    if (1 < uVar1 - 0x10) {
      if (uVar1 - 0x12 < 4) {
        Mode_SaveLast(0xe,g_mode);
        return;
      }
      if (uVar1 != 0x16) {
        if (1 < uVar1 - 0x17) {
          return;
        }
        goto LAB_080111ce;
      }
    }
  }
  Mode_SaveLast(0xd,g_mode);
  return;
}


// ==== Settings_LoadLogInterval @ 08011218

/* EEPROM 0x18 u16, <= 999 else 1. medium */

void Settings_LoadLogInterval(void)

{
  g_log_interval = EE_ReadU16(0x18);
  if (999 < g_log_interval) {
    g_log_interval = 1;
  }
  return;
}


// ==== Settings_SaveLogInterval @ 08011248

/* high */

void Settings_SaveLogInterval(undefined2 param_1)

{
  EE_WriteU16(0x18,param_1);
  return;
}


// ==== Settings_LoadContrast @ 08011258

/* EEPROM 0x1A, 12..17 else 14. high */

void Settings_LoadContrast(void)

{
  g_lcd_contrast = EE_ReadU8(0x1a);
  if ((g_lcd_contrast < 0xc) || (0x11 < g_lcd_contrast)) {
    g_lcd_contrast = 0xe;
  }
  return;
}


// ==== Settings_SaveContrast @ 08011294

/* high */

void Settings_SaveContrast(undefined1 param_1)

{
  EE_WriteByte(0x1a,param_1);
  return;
}


// ==== Settings_LoadApo @ 080112a4

/* EEPROM 0x16 APO on/off. high */

void Settings_LoadApo(void)

{
  byte bVar1;
  
  bVar1 = EE_ReadU8(0x16);
  if (bVar1 < 2) {
    if (bVar1 == 0) {
      g_apo_off = 0;
    }
    else {
      g_apo_off = 1;
    }
  }
  else {
    g_apo_off = 0;
  }
  return;
}


// ==== Settings_SaveApo @ 080112ec

/* EEPROM 0x16 APO on/off. high */

void Settings_SaveApo(undefined1 param_1)

{
  EE_WriteByte(0x16,param_1);
  return;
}


// ==== Settings_LoadTempUnit @ 080112fc

/* EEPROM 0x10 temperature unit. high */

void Settings_LoadTempUnit(void)

{
  g_temp_unit = EE_ReadU8(0x10);
  if (1 < g_temp_unit) {
    g_temp_unit = 0;
  }
  return;
}


// ==== Settings_SaveTempUnit @ 08011330

/* EEPROM 0x10 temperature unit. high */

void Settings_SaveTempUnit(undefined1 param_1)

{
  EE_WriteByte(0x10,param_1);
  return;
}


// ==== Settings_LoadDiodeV @ 08011340

/* EEPROM 0x12 bool -> 200080b3: diode test 15 V / 3 V. medium */

void Settings_LoadDiodeV(void)

{
  byte bVar1;
  
  bVar1 = EE_ReadU8(0x12);
  if (bVar1 < 2) {
    if (bVar1 == 0) {
      g_diode_15v = 0;
    }
    else {
      g_diode_15v = 1;
    }
  }
  else {
    g_diode_15v = 0;
  }
  return;
}


// ==== Settings_SaveDiodeV @ 08011384

/* high */

void Settings_SaveDiodeV(undefined1 param_1)

{
  EE_WriteByte(0x12,param_1);
  return;
}


// ==== Settings_LoadContinuity @ 08011394

/* EEPROM 0x14 continuity mode 0..3. high */

void Settings_LoadContinuity(void)

{
  g_continuity_mode = EE_ReadU8(0x14);
  if (3 < g_continuity_mode) {
    g_continuity_mode = 0;
  }
  return;
}


// ==== Settings_SaveContinuity @ 080113cc

/* high */

void Settings_SaveContinuity(undefined1 param_1)

{
  EE_WriteByte(0x14,param_1);
  return;
}


// ==== Settings_LoadVaBurden @ 080113dc

/* EEPROM 0x1B VA burden display. high */

void Settings_LoadVaBurden(void)

{
  char cVar1;
  
  cVar1 = EE_ReadU8(0x1b);
  g_va_burden_display = cVar1 == '\x01';
  return;
}


// ==== Settings_SaveVaBurden @ 08011400

/* high */

void Settings_SaveVaBurden(undefined1 param_1)

{
  EE_WriteByte(0x1b,param_1);
  return;
}


// ==== Settings_LoadBuzzer @ 08011410

/* EEPROM 0x1E == 1 -> 20008078 buzzer enabled. high */

void Settings_LoadBuzzer(void)

{
  char cVar1;
  
  cVar1 = EE_ReadU8(0x1e);
  g_buzzer_enabled = cVar1 == '\x01';
  return;
}


// ==== Settings_SaveBuzzer @ 08011438

/* high */

void Settings_SaveBuzzer(undefined1 param_1)

{
  EE_WriteByte(0x1e,param_1);
  return;
}


// ==== CurrentZero_Load @ 08011448

/* trim 0x20007e0c from the slot when its marker is 0xA5, else 0; on mode/range change. high */

undefined4 CurrentZero_Load(char param_1,char param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  ushort uVar2;
  
  DAT_20007e0c = 0;
  if (((((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) ||
      (((param_1 == '\x11' || (param_1 == '\x13')) || (param_1 == '\x15')))) && (param_2 == '\0')) {
    uVar2 = CurrentZero_Slot(param_1);
    cVar1 = EE_ReadU8(uVar2);
    if (cVar1 == -0x5b) {
      DAT_20007e0c = EE_ReadU32(uVar2 + 1);
    }
  }
  return param_4;
}


// ==== CurrentZero_Store @ 080114bc

/* (mode 0x10-0x15, range 0, value): EEPROM slot 0xA5 + u32 BE. high */

undefined4 CurrentZero_Store(char param_1,char param_2,undefined4 param_3,undefined4 param_4)

{
  ushort uVar1;
  
  if (((((param_1 == '\x10') || (param_1 == '\x12')) || (param_1 == '\x14')) ||
      (((param_1 == '\x11' || (param_1 == '\x13')) || (param_1 == '\x15')))) && (param_2 == '\0')) {
    uVar1 = CurrentZero_Slot(param_1);
    EE_WriteByte(uVar1,0xa5);
    EE_WriteU32(uVar1 + 1,param_3);
  }
  return param_4;
}


// ==== CurrentZero_ClearAll @ 0801150e

/* all six current-zero slots := 0xA5 + 0; called when calibration mode is entered (MODE+REL at
   boot; was Cal_Enter). high (emulated) */

undefined4 CurrentZero_ClearAll(void)

{
  undefined4 unaff_r7;
  
  CurrentZero_Store(0x10,0,0);
  CurrentZero_Store(0x11,0,0);
  CurrentZero_Store(0x12,0,0);
  CurrentZero_Store(0x13,0,0);
  CurrentZero_Store(0x14,0,0);
  CurrentZero_Store(0x15,0,0);
  return unaff_r7;
}


// ==== CurrentZero_Slot @ 0801154e

/* mode 0x10..0x15 -> EEPROM 0x4D0 0x4D8 0x4E0 0x4E8 0x4F0 0x4F8. high */

ushort CurrentZero_Slot(ushort param_1)

{
  ushort uVar1;
  
  uVar1 = param_1 & 0xff;
  if (uVar1 == 0x10) {
    param_1 = 0x4d0;
  }
  else if (0xf < uVar1) {
    if (uVar1 == 0x12) {
      param_1 = 0x4e0;
    }
    else if (uVar1 < 0x12) {
      param_1 = 0x4d8;
    }
    else if (uVar1 == 0x14) {
      param_1 = 0x4f0;
    }
    else if (uVar1 < 0x14) {
      param_1 = 0x4e8;
    }
    else if (uVar1 == 0x15) {
      param_1 = 0x4f8;
    }
  }
  return param_1;
}


// ==== Cal_RecordAddr @ 0801159c

/* EEPROM cal record offset for (g_mode, g_range): table 0x08028ee8[mode] + range * 16; arg unused.
   high */

undefined8 Cal_RecordAddr(undefined4 param_1)

{
  return CONCAT44(param_1,(uint)(ushort)(*(short *)(&DAT_08028ee8 + (uint)g_mode * 2) +
                                        (ushort)g_range * 0x10));
}


// ==== Cal_AcFreqAddr @ 080115cc

/* (mode, range) EEPROM AC frequency-point offset: table 0x08028ee8[mode] + 8 + range * 16 (modes
   16/18 force range 1). high */

short Cal_AcFreqAddr(byte param_1,byte param_2)

{
  if (param_1 == 0x10) {
    param_2 = 1;
  }
  else if (param_1 == 0x12) {
    param_2 = 1;
  }
  return *(short *)(&DAT_08028ee8 + (uint)param_1 * 2) + 8 + (ushort)param_2 * 0x10;
}


// ==== Cal_LoadRecord @ 0801167c

/* calibration record for (mode/slot, range) [video 0x0801167D] */

undefined4 Cal_LoadRecord(byte param_1,uint param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  if (param_1 == 200) {
    DAT_20007d5c = 0x2e0;
    DAT_20007fbe = EE_ReadU16(0x2e0);
    if ((1000 < DAT_20007fbe) || (DAT_20007fbe < -1000)) {
      DAT_20007fbe = 0;
    }
  }
  else {
    if (((g_cal_mode == '\0') && (param_1 == 9)) && (g_range == '\x06')) {
      DAT_20007e14 = EE_ReadU32(0x178);
      Cal_SanitizeTrim();
    }
    if (((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) {
      Cal_LoadVaRecord();
    }
    else if (((g_mode == '\x16') || (g_mode == '\x17')) || (g_mode == '\x18')) {
      Cal_LoadVaRecord();
    }
    else {
      uVar1 = (uint)param_1;
      if (uVar1 == 6) {
        DAT_20007d5c = 0x100;
        DAT_20007e18 = EE_ReadU32(0x104);
        if ((60000000 < DAT_20007e18) || (DAT_20007e18 < 40000000)) {
          DAT_20007e18 = 0x2ee0032;
        }
      }
      else {
        if (((uVar1 == 2) || (uVar1 == 4)) && (DAT_200080b7 == '\x01')) {
          DAT_20007d5c = 0x2d0;
        }
        else if (param_1 == 0) {
          if (DAT_20008098 == '\0') {
            DAT_20007d5c = 0x70;
          }
          else {
            DAT_20007d5c = 0x30;
          }
          DAT_20007d5c = DAT_20007d5c + 0x30;
        }
        else if (uVar1 == 2) {
          if ((DAT_200080ba == '\0') || (DAT_200080bd == '\0')) {
            DAT_20007d5c = (uint)*(ushort *)(&DAT_08028ee8 + (uint)param_1 * 2) +
                           (param_2 & 0xff) * 0x10;
          }
          else {
            DAT_20007d5c = (param_2 & 0xff) * 0x10 + 0x70;
          }
        }
        else if (uVar1 == 10) {
          DAT_20007d5c = 0x120;
        }
        else {
          DAT_20007d5c = (uint)*(ushort *)(&DAT_08028ee8 + uVar1 * 2) + (param_2 & 0xff) * 0x10;
        }
        g_cal_offset = EE_ReadU32(DAT_20007d5c);
        EE_ReadU32(DAT_20007d5c + 4);
        g_cal_gain = __aeabi_ui2f();
        if (param_1 == 0xc) {
          if ((g_range == '\x04') && (g_pf8_strap_low != '\0')) {
            g_cal_gain = __aeabi_fdiv(g_cal_gain,0x461c4000);
          }
          else {
            g_cal_gain = __aeabi_fdiv(g_cal_gain,0x47c35000);
          }
        }
        else {
          g_cal_gain = __aeabi_fdiv(g_cal_gain,0x49742400);
        }
        Delay_Loop(1);
      }
    }
  }
  return param_4;
}


// ==== Cal_SanitizeTrim @ 08011924

/* trim 0x20007e14 outside +-3000 -> 0; after Cal_LoadRecord / Cal_StoreField (used by
   Ohm_R6Correction). high */

void Cal_SanitizeTrim(void)

{
  if ((3000 < DAT_20007e14) || (DAT_20007e14 < -3000)) {
    DAT_20007e14 = 0;
  }
  return;
}


// ==== Cal_LoadVaRecord @ 08011950

/* from Cal_LoadRecord (VA modes): record T[g_mode]+16*g_range: V offset +0 -> 0x20007e24, V gain +4
   / 1e6 -> 0x20007e2c, I offset +8 -> 0x20007e28, I gain +12 / 1e6 -> 0x20007e30 (gains as unsigned
   u32 -> float). high */

undefined4 Cal_LoadVaRecord(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  DAT_20007d5c = Cal_RecordAddr(0);
  DAT_20007e24 = EE_ReadU32(DAT_20007d5c);
  EE_ReadU32(DAT_20007d5c + 4);
  DAT_20007e2c = __aeabi_ui2f();
  DAT_20007e2c = __aeabi_fdiv(DAT_20007e2c,0x49742400);
  iVar1 = Cal_RecordAddr(1);
  DAT_20007d5c = iVar1 + 8;
  DAT_20007e28 = EE_ReadU32(DAT_20007d5c);
  EE_ReadU32(DAT_20007d5c + 4);
  DAT_20007e30 = __aeabi_ui2f();
  DAT_20007e30 = __aeabi_fdiv(DAT_20007e30,0x49742400);
  return unaff_r7;
}


// ==== Cal_ClampAcFreqPoint @ 080119f4

/* (point 0..3) AC freq-response cal values: points 0/1 (0x20007e4c/50) outside +-20000 and 2/3
   (0x20007e54/58) outside +-10000 reset to 0. high */

void Cal_ClampAcFreqPoint(byte param_1)

{
  if (param_1 == 0) {
    if ((20000 < DAT_20007e4c) || (DAT_20007e4c < -20000)) {
      DAT_20007e4c = 0;
    }
  }
  else if (param_1 == 2) {
    if ((10000 < DAT_20007e54) || (DAT_20007e54 < -10000)) {
      DAT_20007e54 = 0;
    }
  }
  else if (param_1 < 2) {
    if ((20000 < DAT_20007e50) || (DAT_20007e50 < -20000)) {
      DAT_20007e50 = 0;
    }
  }
  else if ((param_1 == 3) && ((10000 < DAT_20007e58 || (DAT_20007e58 < -10000)))) {
    DAT_20007e58 = 0;
  }
  return;
}


// ==== Cal_LoadAcFreqPoints @ 08011aa4

/* from 0800d81e on a mode load (AC uA/mA/A 16/18/20, ACmV 4, ACV 2, LowZ 0 using the ACV range 3
   record): four s16 at T[mode]+8+16*range (AC uA/mA forced to range 1) -> 0x20007e4c/50/54/58, each
   clamped by Cal_ClampAcFreqPoint. high */

undefined4 Cal_LoadAcFreqPoints(void)

{
  undefined4 unaff_r7;
  
  if (g_mode == '\0') {
    DAT_20007d5c = Cal_AcFreqAddr(2,3);
  }
  else {
    DAT_20007d5c = Cal_AcFreqAddr(g_mode,g_range);
  }
  DAT_20007e4c = EE_ReadS16(DAT_20007d5c);
  Cal_ClampAcFreqPoint(0);
  DAT_20007e50 = EE_ReadS16(DAT_20007d5c + 2);
  Cal_ClampAcFreqPoint(1);
  DAT_20007e54 = EE_ReadS16(DAT_20007d5c + 4);
  Cal_ClampAcFreqPoint(2);
  DAT_20007e58 = EE_ReadS16(DAT_20007d5c + 6);
  Cal_ClampAcFreqPoint(3);
  return unaff_r7;
}


// ==== Cal_StoreField @ 08011b40

/* (field, value) EEPROM write for the current mode/range record T[mode]+16*range: 0 offset, 1 gain
   (g_cal_gain x 1e6; cap x 1e4 r4 PF8-low / 1e5; Hz: crystal 0x104), 2 +8, 3 +12 (value*1e4 float
   bits); PEAK 0x2d0/0x2d4; VA -> 08011dce; 200 temp trim s16 0x2e0 (+-1000); 150 Ohm r6 trim u32
   0x178 (+-3000). high */

void Cal_StoreField(char param_1,int param_2)

{
  undefined4 uVar1;
  
  if (param_1 == -0x38) {
    DAT_20007d5c = 0x2e0;
    if ((1000 < DAT_20007fbe) || (DAT_20007fbe < -1000)) {
      DAT_20007fbe = 0;
    }
    EE_WriteU16(0x2e0,DAT_20007fbe);
  }
  else if (param_1 == -0x6a) {
    Cal_SanitizeTrim();
    EE_WriteU32(0x178,DAT_20007e14);
  }
  else if (((g_mode == 0xd) || (g_mode == 0xe)) || (g_mode == 0xf)) {
    Cal_StoreVaField(param_1);
  }
  else if (((g_mode == 0x16) || (g_mode == 0x17)) || (g_mode == 0x18)) {
    Cal_StoreVaField(param_1);
  }
  else if (param_1 == '\0') {
    if (((g_mode == 2) || (g_mode == 4)) && (DAT_200080b7 == '\x01')) {
      DAT_20007d5c = 0x2d0;
    }
    else {
      DAT_20007d5c = (uint)*(ushort *)(&DAT_08028ee8 + (uint)g_mode * 2) + (uint)g_range * 0x10;
    }
    EE_WriteU32(DAT_20007d5c,param_2);
  }
  else if (param_1 == '\x01') {
    if (g_mode == 6) {
      DAT_20007d5c = 0x104;
      EE_WriteU32(0x104,DAT_20007e18);
    }
    else {
      if (((g_mode == 2) || (g_mode == 4)) && (DAT_200080b7 == '\x01')) {
        DAT_20007d5c = 0x2d4;
      }
      else {
        DAT_20007d5c = (uint)g_range * 0x10 + 4 +
                       (uint)*(ushort *)(&DAT_08028ee8 + (uint)g_mode * 2);
      }
      if (g_mode == 0xc) {
        if ((g_range == 4) && (g_pf8_strap_low != '\0')) {
          uVar1 = __aeabi_fmul(0x461c4000,g_cal_gain);
        }
        else {
          uVar1 = __aeabi_fmul(0x47c35000,g_cal_gain);
        }
      }
      else {
        uVar1 = __aeabi_fmul(0x49742400,g_cal_gain);
      }
      uVar1 = __aeabi_f2iz(uVar1);
      EE_WriteU32(DAT_20007d5c,uVar1);
    }
  }
  else if (param_1 == '\x02') {
    DAT_20007d5c = (uint)g_range * 0x10 + 8 + (uint)*(ushort *)(&DAT_08028ee8 + (uint)g_mode * 2);
    EE_WriteU32(DAT_20007d5c,param_2);
  }
  else if (param_1 == '\x03') {
    DAT_20007d5c = (uint)g_range * 0x10 + 0xc + (uint)*(ushort *)(&DAT_08028ee8 + (uint)g_mode * 2);
    uVar1 = __aeabi_i2f(param_2 * 10000);
    uVar1 = __aeabi_f2iz(uVar1);
    EE_WriteU32(DAT_20007d5c,uVar1);
  }
  return;
}


// ==== Cal_StoreVaField @ 08011dce

/* Cal_StoreField for VA modes: field 0 writes both offsets (0x20007e24 -> +0, 0x20007e28 -> +8),
   field 1 both gains x 1e6 (0x20007e2c -> +4, 0x20007e30 -> +12) of record T[g_mode]+16*g_range;
   other fields ignored. high */

undefined4 Cal_StoreVaField(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  
  if (param_1 == '\0') {
    DAT_20007d5c = Cal_RecordAddr(0);
    EE_WriteU32(DAT_20007d5c,DAT_20007e24);
    iVar1 = Cal_RecordAddr(1);
    DAT_20007d5c = iVar1 + 8;
    EE_WriteU32(DAT_20007d5c,DAT_20007e28);
  }
  else if (param_1 == '\x01') {
    iVar1 = Cal_RecordAddr(0);
    DAT_20007d5c = iVar1 + 4;
    uVar2 = __aeabi_fmul(0x49742400,DAT_20007e2c);
    uVar2 = __aeabi_f2iz(uVar2);
    EE_WriteU32(DAT_20007d5c,uVar2);
    iVar1 = Cal_RecordAddr(1);
    DAT_20007d5c = iVar1 + 0xc;
    uVar2 = __aeabi_fmul(0x49742400,DAT_20007e30);
    uVar2 = __aeabi_f2iz(uVar2);
    EE_WriteU32(DAT_20007d5c,uVar2);
  }
  return param_4;
}


// ==== Cal_StoreAcFreqPoints @ 08011e88

/* (mode, range, which): clamps and writes AC freq-response point 1..4 (0x20007e4c/50/54/58) as s16
   at T[mode]+8+16*range (+0/+2/+4/+6); which = 100 writes all four. From Cal_AcFreqPointStore and
   Cal_AcFreqClear. high */

void Cal_StoreAcFreqPoints(undefined1 param_1,undefined1 param_2,char param_3)

{
  DAT_20007d5c = Cal_AcFreqAddr(param_1,param_2);
  if ((param_3 == '\x01') || (param_3 == 'd')) {
    Cal_ClampAcFreqPoint(0);
    EE_WriteU16(DAT_20007d5c,DAT_20007e4c & 0xffff);
  }
  if ((param_3 == '\x02') || (param_3 == 'd')) {
    Cal_ClampAcFreqPoint(1);
    EE_WriteU16(DAT_20007d5c + 2,DAT_20007e50 & 0xffff);
  }
  if ((param_3 == '\x03') || (param_3 == 'd')) {
    Cal_ClampAcFreqPoint(2);
    EE_WriteU16(DAT_20007d5c + 4,DAT_20007e54 & 0xffff);
  }
  if ((param_3 == '\x04') || (param_3 == 'd')) {
    Cal_ClampAcFreqPoint(3);
    EE_WriteU16(DAT_20007d5c + 6,DAT_20007e58 & 0xffff);
  }
  return;
}


// ==== Cal_AcFreqClear @ 08011f38

/* from Cal_AcFreqWindow: clears the four point-captured flags 0x2000809c..f and the four values
   0x20007e4c..58, then writes all four zeros to EEPROM (Cal_StoreAcFreqPoints(.., 100)). high */

undefined4 Cal_AcFreqClear(void)

{
  undefined4 unaff_r7;
  
  DAT_2000809f = 0;
  DAT_2000809e = 0;
  DAT_2000809d = 0;
  DAT_2000809c = 0;
  DAT_20007e58 = 0;
  DAT_20007e54 = 0;
  DAT_20007e50 = 0;
  DAT_20007e4c = 0;
  Cal_StoreAcFreqPoints(g_mode,g_range,100);
  return unaff_r7;
}


// ==== EE_ReadU32 @ 08011f80

/* big-endian word at 0x08080000 + off. high */

undefined8 EE_ReadU32(int param_1)

{
  undefined1 uVar1;
  uint uVar2;
  uint local_18 [2];
  
  for (uVar2 = 0; (uVar2 & 0xff) < 4; uVar2 = (uVar2 & 0xff) + 1) {
    uVar1 = EE_ReadByte((uVar2 & 0xff) + param_1);
    *(undefined1 *)((int)local_18 + (uVar2 & 0xff)) = uVar1;
  }
  return CONCAT44(local_18[0],
                  (local_18[0] >> 0x18) +
                  (local_18[0] >> 0x10 & 0xff) * 0x100 +
                  (local_18[0] >> 8 & 0xff) * 0x10000 + local_18[0] * 0x1000000);
}


// ==== EE_ReadS16 @ 08011fd4

/* (offset) EE_ReadU16 sign-extended to int; Cal_LoadAcFreqPoints. high */

uint EE_ReadS16(undefined4 param_1)

{
  uint uVar1;
  
  uVar1 = EE_ReadU16(param_1);
  uVar1 = uVar1 & 0xffff;
  if (0x7fff < uVar1) {
    uVar1 = uVar1 - 0x10000;
  }
  return uVar1;
}


// ==== EE_ReadU16 @ 08012000

/* big-endian halfword at 0x08080000 + off. high */

undefined8 EE_ReadU16(int param_1)

{
  undefined1 uVar1;
  uint uVar2;
  undefined4 local_18;
  
  for (uVar2 = 0; (uVar2 & 0xff) < 2; uVar2 = (uVar2 & 0xff) + 1) {
    uVar1 = EE_ReadByte((uVar2 & 0xff) + param_1);
    *(undefined1 *)((int)&local_18 + (uVar2 & 0xff)) = uVar1;
  }
  return CONCAT44(local_18,(uint)CONCAT11((undefined1)local_18,local_18._1_1_));
}


// ==== EE_ReadU8 @ 0801203a

/* (offset) EE_ReadByte & 0xFF; settings, variant flags, last-mode loaders. high */

ulonglong EE_ReadU8(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  
  uVar1 = EE_ReadByte(param_1);
  return CONCAT44(param_4,uVar1) & 0xffffffff000000ff;
}


// ==== EE_ReadByte @ 0801204c

/* *(0x08080000 + off) & 0xFF. high */

uint EE_ReadByte(int param_1)

{
  return *(uint *)(&DAT_08080000 + param_1) & 0xff;
}


// ==== EE_WriteU32 @ 08012068

/* big-endian. high */

void EE_WriteU32(int param_1,uint param_2)

{
  EE_WriteByteRetry(param_1,param_2 >> 0x18);
  EE_WriteByteRetry(param_1 + 1,param_2 >> 0x10 & 0xff);
  EE_WriteByteRetry(param_1 + 2,param_2 >> 8 & 0xff);
  EE_WriteByteRetry(param_1 + 3,param_2 & 0xff);
  return;
}


// ==== EE_WriteU16 @ 080120a8

/* big-endian. high */

void EE_WriteU16(int param_1,undefined2 param_2)

{
  EE_WriteByteRetry(param_1,(char)((ushort)param_2 >> 8));
  EE_WriteByteRetry(param_1 + 1,(char)param_2);
  return;
}


// ==== EE_WriteByte @ 080120d0

/* high */

undefined4 EE_WriteByte(undefined4 param_1,undefined1 param_2,undefined4 param_3,undefined4 param_4)

{
  EE_WriteByteRetry(param_1,param_2);
  return param_4;
}


// ==== EE_ProgramByte @ 080120f0

/* unlock, DATA_EEPROM_ProgramByte (strb), lock. high */

undefined1 EE_ProgramByte(int param_1,undefined1 param_2)

{
  undefined1 uVar1;
  
  DATA_EEPROM_Unlock();
  uVar1 = DATA_EEPROM_ProgramByte(&DAT_08080000 + param_1,param_2);
  DATA_EEPROM_Lock();
  return uVar1;
}


// ==== EE_WriteByteRetry @ 08012148

/* up to 10 tries until FLASH status complete. high */

void EE_WriteByteRetry(undefined4 param_1,undefined1 param_2)

{
  char cVar1;
  int iVar2;
  
  iVar2 = 0;
  while ((iVar2 < 10 && (cVar1 = EE_ProgramByte(param_1,param_2), cVar1 != '\x04'))) {
    iVar2 = iVar2 + 1;
  }
  return;
}


// ==== Record_Store @ 0801217c

/* (slot) stores the current record into the 60-slot RAM ring: date/time 0x200080d7..dc, main
   code/flags 0x20007fee/ef + value 0x20007d28, sub code/flags 0x20007ff0/f1 + 0x20007d2c, third
   0x20007ff2/f3 + 0x20007d30 (column arrays 0x20007810..0x20007aa4, u32 arrays
   0x200073c8/74b8/75a8); inverse of Record_Load. high */

void Record_Store(ushort param_1)

{
  (&DAT_20007810)[param_1] = DAT_200080d7;
  (&DAT_2000784c)[param_1] = DAT_200080d8;
  (&DAT_20007888)[param_1] = DAT_200080d9;
  (&DAT_200078c4)[param_1] = DAT_200080da;
  (&DAT_20007900)[param_1] = DAT_200080db;
  (&DAT_2000793c)[param_1] = DAT_200080dc;
  (&DAT_20007978)[param_1] = DAT_20007fee;
  (&DAT_200079b4)[param_1] = DAT_20007fef;
  *(undefined4 *)(&DAT_200073c8 + (uint)param_1 * 4) = DAT_20007d28;
  (&DAT_200079f0)[param_1] = DAT_20007ff0;
  (&DAT_20007a2c)[param_1] = DAT_20007ff1;
  *(undefined4 *)(&DAT_200074b8 + (uint)param_1 * 4) = DAT_20007d2c;
  (&DAT_20007a68)[param_1] = DAT_20007ff2;
  (&DAT_20007aa4)[param_1] = DAT_20007ff3;
  *(undefined4 *)(&DAT_200075a8 + (uint)param_1 * 4) = DAT_20007d30;
  return;
}


// ==== Record_Load @ 08012260

/* (index) unpack stored record from the RAM ring (60-byte column arrays 0x20007810..0x20007aa4, u32
   values 0x200073c8/0x200074b8/0x200075a8): date/time 0x20008007..0c, main code -> mode 0x2000801e
   / range 0x2000801f, flags -> 0x20007fe5..ea, values 0x20007d1c/20/24; inverse of 0801217c. Used
   by memory recall and the logger. high */

void Record_Load(ushort param_1)

{
  DAT_20008007 = (&DAT_20007810)[param_1];
  DAT_20008008 = (&DAT_2000784c)[param_1];
  DAT_20008009 = (&DAT_20007888)[param_1];
  DAT_2000800a = (&DAT_200078c4)[param_1];
  DAT_2000800b = (&DAT_20007900)[param_1];
  DAT_2000800c = (&DAT_2000793c)[param_1];
  DAT_20007ff4 = (&DAT_20007978)[param_1];
  DAT_2000801e = DAT_20007ff4 & 0x1f;
  DAT_2000801f = DAT_20007ff4 >> 5;
  DAT_20007ff5 = (&DAT_200079b4)[param_1];
  DAT_20007fe5 = (DAT_20007ff5 & 1) != 0;
  DAT_20007fe6 = (DAT_20007ff5 & 2) != 0;
  DAT_20007fe7 = (DAT_20007ff5 & 4) != 0;
  DAT_20007fe8 = (DAT_20007ff5 & 8) != 0;
  DAT_20007fe9 = (DAT_20007ff5 & 0x10) != 0;
  DAT_20007fea = (DAT_20007ff5 & 0x20) != 0;
  DAT_20007d1c = *(undefined4 *)(&DAT_200073c8 + (uint)param_1 * 4);
  DAT_20007ff6 = (&DAT_200079f0)[param_1];
  DAT_20008020 = DAT_20007ff6 & 0x1f;
  DAT_20008021 = DAT_20007ff6 >> 5;
  DAT_20007ff7 = (&DAT_20007a2c)[param_1];
  DAT_20007d20 = *(undefined4 *)(&DAT_200074b8 + (uint)param_1 * 4);
  DAT_20007ff8 = (&DAT_20007a68)[param_1];
  DAT_20008022 = DAT_20007ff8 & 0x1f;
  DAT_20008023 = DAT_20007ff8 >> 5;
  DAT_20007ff9 = (&DAT_20007aa4)[param_1];
  DAT_20007d24 = *(undefined4 *)(&DAT_200075a8 + (uint)param_1 * 4);
  return;
}


// ==== AcFreq_Correction @ 080124f4  [DAT]

/* (reading, cal pts 1-4, type, counter, counter range, variant_acv0) frequency-response correction
   (counts) for AC: type 0/1 ACmV r0/1, 2/3 ACV r0/1 (-> AcFreq_CorrectionACV), 4/5 ACV r2/r3, 6/7
   AC uA/mA (AC A type 8 rejected); only freq > 5.0 Hz, reading < 55000 (600 V r: < 6600);
   interpolates breakpoint tables 0x0802900c / 0x080290f0 and per-level percent tables 0x08028a14..
   / 0x08028e04.. scaled by the stored points. medium */

int AcFreq_Correction(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                     undefined4 param_5,byte param_6,undefined4 param_7,undefined1 param_8,
                     undefined1 param_9)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  undefined8 uVar7;
  
  iVar6 = 9;
  DAT_20007f0c = 0;
  DAT_20007f44 = 0;
  if ((((param_6 != 5) || (param_1 < 0x19c8)) && ((param_6 != 4 || (param_1 < 60000)))) &&
     (((param_1 < 55000 && (param_6 < 8)) &&
      (DAT_20007f10 = Freq_ToDeciHz(param_8,param_7), 0x32 < DAT_20007f10)))) {
    if ((param_6 == 2) || (param_6 == 3)) {
      DAT_20007f0c = AcFreq_CorrectionACV
                               (param_1,param_2,param_3,param_4,param_5,param_6,DAT_20007f10,param_9
                               );
    }
    else {
      if (3 < param_6) {
        iVar6 = 7;
      }
      for (iVar1 = 0; iVar4 = iVar6 + -1, iVar1 < iVar6; iVar1 = iVar1 + 1) {
        iVar4 = iVar1;
        if (param_6 < 2) {
          if (DAT_20007f10 <= (int)(&DAT_0802900c)[iVar1]) break;
        }
        else if ((3 < param_6) && (DAT_20007f10 <= (int)(&DAT_080290f0)[iVar1])) break;
      }
      if (param_6 < 2) {
        DAT_20007f30 = (&DAT_0802900c)[iVar4];
        DAT_20007f34 = (&DAT_08029008)[iVar4];
      }
      else if (3 < param_6) {
        DAT_20007f30 = (&DAT_080290f0)[iVar4];
        DAT_20007f34 = (&DAT_080290ec)[iVar4];
      }
      DAT_20007f18 = DAT_20007f30 - DAT_20007f34;
      DAT_20007f1c = DAT_20007f10 - DAT_20007f34;
      DAT_20007f3c = param_2;
      DAT_20007f40 = param_3;
      uVar2 = __aeabi_i2f(DAT_20007f1c);
      uVar3 = __aeabi_i2f(DAT_20007f18);
      DAT_20007f48 = __aeabi_fdiv(uVar2,uVar3);
      uVar5 = (uint)(5000 < param_1);
      if (param_6 == 0) {
        DAT_20007f30 = (&DAT_08028a18)[uVar5 * 9 + iVar4];
        DAT_20007f34 = (&DAT_08028a14)[iVar4 + uVar5 * 9];
      }
      else if (param_6 == 1) {
        DAT_20007f30 = (&DAT_08028a60)[uVar5 * 9 + iVar4];
        DAT_20007f34 = (&DAT_08028a5c)[iVar4 + uVar5 * 9];
      }
      else if (param_6 == 4) {
        DAT_20007f30 = (&DAT_08028e08)[uVar5 * 7 + iVar4];
        DAT_20007f34 = (&DAT_08028e04)[iVar4 + uVar5 * 7];
      }
      else if (param_6 == 5) {
        DAT_20007f30 = (&DAT_08028e40)[uVar5 * 7 + iVar4];
        DAT_20007f34 = (&DAT_08028e3c)[iVar4 + uVar5 * 7];
      }
      else if (param_6 == 6) {
        DAT_20007f30 = (&DAT_08028e78)[uVar5 * 7 + iVar4];
        DAT_20007f34 = (&DAT_08028e74)[iVar4 + uVar5 * 7];
      }
      else if (param_6 == 7) {
        DAT_20007f30 = (&DAT_08028eb0)[uVar5 * 7 + iVar4];
        DAT_20007f34 = (&DAT_08028eac)[iVar4 + uVar5 * 7];
      }
      iVar6 = DAT_20007f34;
      DAT_20007f18 = DAT_20007f34;
      DAT_20007f1c = DAT_20007f30 - DAT_20007f34;
      uVar2 = __aeabi_i2f(DAT_20007f1c);
      __aeabi_fmul(uVar2,DAT_20007f48);
      uVar7 = __aeabi_f2d();
      __aeabi_dadd((int)uVar7,(int)((ulonglong)uVar7 >> 0x20),0,0x3fe00000);
      iVar1 = __aeabi_d2iz();
      DAT_20007f14 = iVar1 + iVar6;
      uVar2 = __aeabi_i2f(DAT_20007f3c);
      uVar3 = __aeabi_i2f(DAT_20007f14);
      uVar2 = __aeabi_fmul(uVar3,uVar2);
      __aeabi_fdiv(uVar2,&DAT_42c80000);
      DAT_20007f20 = __aeabi_f2iz();
      uVar2 = __aeabi_i2f(DAT_20007f40);
      uVar3 = __aeabi_i2f(DAT_20007f14);
      uVar2 = __aeabi_fmul(uVar3,uVar2);
      __aeabi_fdiv(uVar2,&DAT_42c80000);
      DAT_20007f24 = __aeabi_f2iz();
      if (param_6 == 5) {
        DAT_20007f28 = DAT_20007f20 + 600;
        DAT_20007f2c = DAT_20007f24 + 0x1518;
        DAT_20007f38 = 600;
      }
      else {
        DAT_20007f28 = DAT_20007f20 + 5000;
        DAT_20007f2c = DAT_20007f24 + 45000;
        DAT_20007f38 = 5000;
      }
      if (DAT_20007f28 < param_1) {
        DAT_20007f34 = DAT_20007f24 - DAT_20007f20;
        DAT_20007f30 = DAT_20007f24;
        uVar2 = __aeabi_i2f(param_1 - DAT_20007f38);
        uVar3 = __aeabi_i2f(DAT_20007f2c);
        __aeabi_fdiv(uVar2,uVar3);
        uVar7 = __aeabi_f2d();
        __aeabi_dadd((int)uVar7,(int)((ulonglong)uVar7 >> 0x20),0,0xbff00000);
        DAT_20007f4c = __aeabi_d2f();
        uVar2 = __aeabi_i2f(DAT_20007f34);
        DAT_20007f4c = __aeabi_fmul(uVar2,DAT_20007f4c);
      }
      else {
        DAT_20007f30 = 0;
        uVar2 = __aeabi_i2f(param_1);
        uVar3 = __aeabi_i2f(DAT_20007f28);
        uVar2 = __aeabi_fdiv(uVar2,uVar3);
        uVar3 = __aeabi_i2f(DAT_20007f20);
        DAT_20007f4c = __aeabi_fmul(uVar3,uVar2);
      }
      iVar6 = DAT_20007f30;
      iVar1 = __aeabi_f2iz(DAT_20007f4c);
      DAT_20007f0c = DAT_20007f44 + iVar1 + iVar6;
    }
  }
  return DAT_20007f0c;
}


// ==== AcFreq_CorrectionACV @ 080129ee  [DAT]

/* ACV ranges 0/1 part of AcFreq_Correction: 9-point frequency tables split at 2000.0 Hz (0x08029030
   / 0x08029054), two-level percent tables (0x08028aa4.. variant_acv0 ACV r0, 0x08028b34.., r1
   0x08028bc4..), interpolated between the 10% and 100% cal points. medium */

int AcFreq_CorrectionACV
              (int param_1,int param_2,int param_3,int param_4,int param_5,char param_6,int param_7,
              char param_8)

{
  undefined4 uVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined8 uVar6;
  
  DAT_20007f54 = param_3 - param_5;
  DAT_20007f58 = param_2 - param_4;
  for (iVar4 = 0; iVar5 = 8, iVar4 < 9; iVar4 = iVar4 + 1) {
    iVar5 = iVar4;
    if (param_7 < 0x4e21) {
      if (param_7 <= (int)(&DAT_08029030)[iVar4]) break;
    }
    else if (param_7 <= (int)(&DAT_08029054)[iVar4]) break;
  }
  if (param_7 < 0x4e21) {
    DAT_20007f74 = (&DAT_08029030)[iVar5];
    DAT_20007f78 = (&DAT_0802902c)[iVar5];
  }
  else {
    DAT_20007f74 = (&DAT_08029054)[iVar5];
    DAT_20007f78 = (&DAT_08029050)[iVar5];
  }
  DAT_20007f64 = DAT_20007f74 - DAT_20007f78;
  DAT_20007f68 = param_7 - DAT_20007f78;
  uVar1 = __aeabi_i2f(DAT_20007f68);
  uVar2 = __aeabi_i2f(DAT_20007f64);
  DAT_20007f94 = __aeabi_fdiv(uVar1,uVar2);
  if (param_6 == '\x02') {
    if (param_8 == '\0') {
      if (param_7 < 0x4e21) {
        DAT_20007f74 = (&DAT_08028b38)[iVar5];
        DAT_20007f78 = (&DAT_08028b34)[iVar5];
        DAT_20007f7c = (&DAT_08028b5c)[iVar5];
        DAT_20007f80 = (&DAT_08028b58)[iVar5];
      }
      else {
        DAT_20007f74 = (&DAT_08028b80)[iVar5];
        DAT_20007f78 = (&DAT_08028b7c)[iVar5];
        DAT_20007f7c = (&DAT_08028ba4)[iVar5];
        DAT_20007f80 = (&DAT_08028ba0)[iVar5];
      }
    }
    else if (param_7 < 0x4e21) {
      DAT_20007f74 = (&DAT_08028aa8)[iVar5];
      DAT_20007f78 = (&DAT_08028aa4)[iVar5];
      DAT_20007f7c = (&DAT_08028acc)[iVar5];
      DAT_20007f80 = (&DAT_08028ac8)[iVar5];
    }
    else {
      DAT_20007f74 = (&DAT_08028af0)[iVar5];
      DAT_20007f78 = (&DAT_08028aec)[iVar5];
      DAT_20007f7c = (&DAT_08028b14)[iVar5];
      DAT_20007f80 = (&DAT_08028b10)[iVar5];
    }
  }
  else if (param_6 == '\x03') {
    if (param_7 < 0x4e21) {
      DAT_20007f74 = (&DAT_08028bc8)[iVar5];
      DAT_20007f78 = (&DAT_08028bc4)[iVar5];
      DAT_20007f7c = (&DAT_08028bec)[iVar5];
      DAT_20007f80 = (&DAT_08028be8)[iVar5];
    }
    else {
      DAT_20007f74 = (&DAT_08028c10)[iVar5];
      DAT_20007f78 = (&DAT_08028c0c)[iVar5];
      DAT_20007f7c = (&DAT_08028c34)[iVar5];
      DAT_20007f80 = (&DAT_08028c30)[iVar5];
    }
  }
  iVar4 = DAT_20007f78;
  DAT_20007f64 = DAT_20007f78;
  DAT_20007f68 = DAT_20007f74 - DAT_20007f78;
  uVar1 = __aeabi_i2f(DAT_20007f68);
  __aeabi_fmul(uVar1,DAT_20007f94);
  uVar6 = __aeabi_f2d();
  __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
  iVar3 = __aeabi_d2iz();
  iVar5 = DAT_20007f80;
  DAT_20007f5c = iVar3 + iVar4;
  DAT_20007f64 = DAT_20007f80;
  DAT_20007f68 = DAT_20007f7c - DAT_20007f80;
  uVar1 = __aeabi_i2f(DAT_20007f68);
  __aeabi_fmul(uVar1,DAT_20007f94);
  uVar6 = __aeabi_f2d();
  __aeabi_dadd((int)uVar6,(int)((ulonglong)uVar6 >> 0x20),0,0x3fe00000);
  iVar4 = __aeabi_d2iz();
  DAT_20007f60 = iVar4 + iVar5;
  if (param_7 < 0x4e21) {
    DAT_20007f84 = (DAT_20007f5c * param_4) / 100;
    DAT_20007f88 = (DAT_20007f60 * param_5) / 100;
  }
  else {
    DAT_20007f84 = param_4 + (DAT_20007f5c * DAT_20007f58) / 100;
    DAT_20007f88 = param_5 + (DAT_20007f60 * DAT_20007f54) / 100;
  }
  DAT_20007f70 = param_4 + 5000;
  if (DAT_20007f70 < param_1) {
    uVar1 = __aeabi_i2f(param_1);
    uVar2 = __aeabi_i2f(DAT_20007f70);
    uVar1 = __aeabi_fsub(uVar1,uVar2);
    uVar2 = __aeabi_i2f(DAT_20007f70);
    uVar2 = __aeabi_fsub(0x47435000,uVar2);
    uVar1 = __aeabi_fdiv(uVar1,uVar2);
    __aeabi_fmul(uVar1,&DAT_42c80000);
    DAT_20007f6c = __aeabi_f2iz();
    DAT_20007f90 = DAT_20007f88 - DAT_20007f84;
    DAT_20007f8c = DAT_20007f88 - (DAT_20007f90 - (DAT_20007f6c * DAT_20007f90) / 100);
  }
  else {
    uVar1 = __aeabi_i2f(DAT_20007f70);
    uVar2 = __aeabi_i2f(param_1);
    uVar1 = __aeabi_fsub(uVar1,uVar2);
    uVar2 = __aeabi_i2f(DAT_20007f70);
    uVar1 = __aeabi_fdiv(uVar1,uVar2);
    __aeabi_fmul(uVar1,&DAT_42c80000);
    DAT_20007f6c = __aeabi_f2iz();
    DAT_20007f90 = DAT_20007f84;
    DAT_20007f8c = DAT_20007f84 - (DAT_20007f6c * DAT_20007f84) / 100;
  }
  DAT_20007f50 = DAT_20007f8c;
  return DAT_20007f8c;
}


// ==== Freq_ToDeciHz @ 08012f28

/* counter value scaled by its range (/1000, /100, /10, x1, x10). medium */

int Freq_ToDeciHz(byte param_1,int param_2)

{
  if (param_1 == 0) {
    param_2 = param_2 / 1000;
  }
  else if (param_1 == 2) {
    param_2 = param_2 / 10;
  }
  else if (param_1 < 2) {
    param_2 = param_2 / 100;
  }
  else if (param_1 == 4) {
    param_2 = param_2 * 10;
  }
  return param_2;
}


// ==== __aeabi_i2f @ 08012f60

/* library (emu_call: -5 -> -5.0) */

int __aeabi_i2f(int param_1)

{
  uint uVar1;
  int iVar2;
  code *UNRECOVERED_JUMPTABLE;
  
  if (param_1 < 0) {
    uVar1 = __iar_ui2f_core(-param_1);
                    /* WARNING: Could not recover jumptable at 0x08012f72. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    iVar2 = (*UNRECOVERED_JUMPTABLE)(uVar1 | 0x80000000);
    return iVar2;
  }
  iVar2 = 0;
  if (param_1 != 0) {
    uVar1 = param_1 << LZCOUNT(param_1);
    iVar2 = (0x9d - LZCOUNT(param_1)) * 0x800000 +
            (uVar1 >> 8) +
            (uint)(0x80000000 < uVar1 * 0x1000000 ||
                  uVar1 * 0x1000000 + 0x80000000 < (uint)((uVar1 & 0x100) != 0));
  }
  return iVar2;
}


// ==== __aeabi_ui2f @ 08012f74

/* library (0xFFFFFFFF -> 4.29e9) */

int __aeabi_ui2f(int param_1)

{
  uint uVar1;
  int iVar2;
  
  iVar2 = 0;
  if (param_1 != 0) {
    uVar1 = param_1 << LZCOUNT(param_1);
    iVar2 = (0x9d - LZCOUNT(param_1)) * 0x800000 +
            (uVar1 >> 8) +
            (uint)(0x80000000 < uVar1 * 0x1000000 ||
                  uVar1 * 0x1000000 + 0x80000000 < (uint)((uVar1 & 0x100) != 0));
  }
  return iVar2;
}


// ==== __iar_ui2f_core @ 08012f78

/* library: interior entry of __aeabi_ui2f past its zero test (normalize, round to float); called by
   __aeabi_i2f on -x. library */

int __iar_ui2f_core(int param_1)

{
  uint uVar1;
  char in_ZR;
  
  if (in_ZR == '\0') {
    uVar1 = param_1 << LZCOUNT(param_1);
    param_1 = (0x9d - LZCOUNT(param_1)) * 0x800000 +
              (uVar1 >> 8) +
              (uint)(0x80000000 < uVar1 * 0x1000000 ||
                    uVar1 * 0x1000000 + 0x80000000 < (uint)((uVar1 & 0x100) != 0));
  }
  return param_1;
}


// ==== __aeabi_fdiv @ 08012f94

/* library (6.0, 2.0 -> 3.0) */

uint __aeabi_fdiv(uint param_1,uint param_2,uint param_3)

{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  uint uVar6;
  uint uVar7;
  int iVar8;
  uint uVar9;
  bool bVar10;
  
  uVar6 = 0xff;
  uVar7 = param_1 >> 0x17 & 0xff;
  bVar10 = uVar7 == 0;
  if (!bVar10) {
    param_3 = param_2 >> 0x17 & 0xff;
    bVar10 = param_3 == 0;
  }
  if (!bVar10) {
    bVar10 = uVar7 == 0xff;
  }
  if (!bVar10) {
    bVar10 = param_3 == 0xff;
  }
  if (bVar10) {
    if (uVar7 != 0xff) {
      param_3 = param_2 >> 0x17 & 0xff;
    }
    if (uVar7 != 0xff && param_3 != 0xff) {
      uVar6 = (param_1 ^ param_2) & 0x80000000;
      if ((param_1 & 0x7fffffff) != 0) {
        if ((param_2 & 0x7fffffff) == 0) goto LAB_08013038;
        if (param_3 == 0) {
          iVar5 = LZCOUNT(param_2 << 9);
          uVar2 = (param_2 << 9) << iVar5;
          if (uVar7 != 0) {
            iVar5 = uVar7 + iVar5;
            uVar7 = param_1 << 8 | 0x80000000;
            goto LAB_08012fba;
          }
        }
        else {
          uVar2 = param_2 << 8 | 0x80000000;
          iVar5 = -param_3;
        }
        iVar8 = LZCOUNT(param_1 << 9);
        uVar7 = (param_1 << 9) << iVar8;
        iVar5 = iVar5 - iVar8;
        goto LAB_08012fba;
      }
      bVar10 = (param_2 & 0x7fffffff) == 0;
    }
    else {
      if (uVar7 != 0xff) {
        if ((param_2 & 0x7fffff) == 0) {
          param_2 = (param_1 ^ param_2) & 0x80000000;
        }
        return param_2;
      }
      bVar10 = param_3 == 0xff;
      param_3 = 0;
      if (!bVar10) {
        return param_1 ^ param_2 & 0x80000000;
      }
    }
    if (bVar10) {
      uVar6 = ~param_3;
    }
  }
  else {
    iVar5 = uVar7 - param_3;
    uVar6 = (param_1 ^ param_2) & 0x80000000;
    uVar2 = param_2 << 8 | 0x80000000;
    uVar7 = param_1 << 8 | 0x80000000;
LAB_08012fba:
    uVar3 = uVar7 - uVar2;
    if (uVar7 < uVar2) {
      iVar5 = iVar5 + -1;
      uVar3 = uVar2 + uVar3 * 2;
    }
    uVar2 = uVar2 >> 8;
    uVar4 = (uVar3 - uVar2 * (uVar3 / uVar2)) * 0x100;
    uVar7 = uVar4 / uVar2;
    uVar4 = (uVar4 - uVar2 * uVar7) * 0x100;
    uVar9 = uVar4 / uVar2;
    uVar7 = uVar9 | (uVar7 & 0xffff00ff | (uVar3 / uVar2 & 0xff) << 8) << 8;
    iVar8 = iVar5 + 0x7f;
    if (iVar8 != 0 && -0x80 < iVar5) {
      if (iVar8 < 0xfe) {
        return (uVar6 | uVar7 >> 1) + iVar8 * 0x800000 + (uint)((byte)uVar9 & 1);
      }
LAB_08013038:
      return uVar6 | 0x7f800000;
    }
    uVar3 = 2 - iVar8;
    uVar7 = uVar7 | 0x1000000;
    if ((int)uVar3 < 0x1a) {
      uVar1 = 0x20 - uVar3 & 0xff;
      uVar2 = uVar7 << uVar1 | uVar4 - uVar2 * uVar9;
      return (uVar7 >> (uVar3 & 0xff)) + uVar6 +
             (uint)(0x80000000 < uVar2 ||
                   uVar2 + 0x80000000 <
                   (uint)(uVar1 == 0 && 0x18 < uVar3 ||
                         uVar1 != 0 && (uVar7 << uVar1 - 1 & 0x80000000) != 0));
    }
  }
  return uVar6;
}


// ==== __aeabi_fmul @ 08013090

/* IAR runtime float multiply (run with emu_call.py). library */

ulonglong __aeabi_fmul(uint param_1,uint param_2,undefined4 param_3,uint param_4)

{
  bool bVar1;
  ulonglong uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  uint uVar7;
  bool bVar8;
  
  uVar7 = param_1 >> 0x17 & 0xff;
  bVar8 = uVar7 == 0;
  if (!bVar8) {
    param_4 = param_2 >> 0x17 & 0xff;
    bVar8 = param_4 == 0;
  }
  if (!bVar8) {
    bVar8 = uVar7 == 0xff;
  }
  if (!bVar8) {
    bVar8 = param_4 == 0xff;
  }
  if (bVar8) {
    if (uVar7 != 0xff) {
      param_4 = param_2 >> 0x17 & 0xff;
    }
    uVar4 = (param_1 ^ param_2) & 0x80000000;
    if (uVar7 == 0xff || param_4 == 0xff) {
      param_1 = param_1 * 2;
      if ((param_1 == 0) || (param_2 = param_2 * 2, param_2 == 0)) {
LAB_08013164:
        return CONCAT44(param_2,~uVar4);
      }
      if (param_1 < 0xff000001) {
        param_1 = param_2;
      }
      if (0xff000000 < param_1) goto LAB_08013164;
      goto LAB_0801315e;
    }
    uVar3 = param_2;
    if (((param_1 & 0x7fffffff) == 0) || (uVar3 = param_2 << 1, uVar3 == 0)) goto LAB_08013146;
    if (param_4 == 0) {
      if (uVar7 == 0) goto LAB_08013146;
      iVar6 = LZCOUNT(param_2 << 9);
      uVar3 = (param_2 << 9) << iVar6;
      iVar6 = uVar7 - iVar6;
      uVar7 = param_1 << 8 | 0x80000000;
    }
    else {
      iVar6 = LZCOUNT(param_1 << 9);
      uVar7 = (param_1 << 9) << iVar6;
      iVar6 = param_4 - iVar6;
      uVar3 = param_2 << 8 | 0x80000000;
    }
  }
  else {
    iVar6 = uVar7 + param_4;
    uVar4 = (param_1 ^ param_2) & 0x80000000;
    uVar7 = param_1 << 8 | 0x80000000;
    uVar3 = param_2 << 8 | 0x80000000;
  }
  uVar2 = (ulonglong)uVar3 * (ulonglong)uVar7;
  uVar7 = (uint)(uVar2 >> 0x20);
  bVar8 = (uVar2 & 0x8000000000000000) == 0;
  if (bVar8) {
    uVar7 = uVar7 << 1;
  }
  param_2 = (iVar6 + -0x7f) - (uint)bVar8;
  if ((int)param_2 < 0 != (SBORROW4(iVar6,0x7f) != SBORROW4(iVar6 + -0x7f,(uint)bVar8))) {
    param_2 = 8 - param_2;
    uVar3 = 0x20 - param_2;
    if ((int)param_2 < 0x21) {
      bVar1 = (uVar2 & 1) != 0;
      uVar3 = uVar3 & 0xff;
      bVar8 = (uVar7 << uVar3 - 1 & 0x80000000) != 0;
      uVar5 = (uint)uVar2 >> 1 | uVar7 << uVar3;
      return CONCAT44((uVar5 + 0x80000000) -
                      (uint)((uVar3 != 0 || !bVar1) && (uVar3 == 0 || !bVar8)),
                      (uVar7 >> (param_2 & 0xff)) + uVar4 +
                      (uint)(0x80000000 < uVar5 ||
                            uVar5 + 0x80000000 < (uint)(uVar3 == 0 && bVar1 || uVar3 != 0 && bVar8))
                     );
    }
LAB_08013146:
    return CONCAT44(uVar3,uVar4);
  }
  if ((int)param_2 < 0xfe) {
    uVar4 = uVar4 | param_2 * 0x800000;
    uVar3 = (uint)uVar2 >> 1 | uVar7 << 0x18;
    return CONCAT44(uVar4,uVar4 + (uVar7 >> 8) +
                                  (uint)(0x80000000 < uVar3 ||
                                        uVar3 + 0x80000000 < (uint)((byte)(uVar7 >> 8) & 1)));
  }
LAB_0801315e:
  return CONCAT44(param_2,uVar4) | 0x7f800000;
}


// ==== __aeabi_f2d @ 08013168

/* library */

longlong __aeabi_f2d(uint param_1)

{
  int iVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  
  uVar4 = param_1 >> 0x17 & 0xff;
  if (uVar4 != 0 && uVar4 != 0xff) {
    return CONCAT44(((int)param_1 >> 3 & 0x8fffffffU) + 0x38000000,param_1 << 0x1d);
  }
  if (uVar4 == 0) {
    uVar4 = param_1 & 0x80000000;
    iVar1 = param_1 << 8;
    iVar3 = 0;
    if (iVar1 != 0) {
      uVar2 = iVar1 << LZCOUNT(iVar1);
      uVar4 = uVar4 + 0x38000000 + LZCOUNT(iVar1) * -0x100000 + (uVar2 >> 0xb);
      iVar3 = uVar2 << 0x15;
    }
    return CONCAT44(uVar4,iVar3);
  }
  if ((param_1 & 0x7fffff) == 0) {
    param_1 = param_1 | 0x700000;
  }
  else {
    param_1 = 0xffffffff;
  }
  return (ulonglong)param_1 << 0x20;
}


// ==== __aeabi_dadd @ 080131b8

/* library */

longlong __aeabi_dadd(uint param_1,uint param_2,uint param_3,uint param_4)

{
  bool bVar1;
  byte bVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  bool bVar11;
  
  if (-1 < (int)(param_2 ^ param_4)) {
    uVar4 = param_1 - param_3;
    iVar9 = (param_2 - param_4) - (uint)(param_3 > param_1);
    if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
      bVar11 = param_1 < uVar4;
      param_1 = param_1 - uVar4;
      param_2 = (param_2 - iVar9) - (uint)bVar11;
      bVar11 = CARRY4(param_3,uVar4);
      param_3 = param_3 + uVar4;
      param_4 = param_4 + iVar9 + (uint)bVar11;
    }
    if (param_2 << 1 < 0xffe00000 && 0x1fffff < param_4 * 2) {
      uVar4 = (param_2 >> 0x14) - (param_4 >> 0x14);
      if (0x35 < (int)uVar4) goto LAB_08013298;
      param_4 = (param_4 << 0xb | 0x80000000) >> 0xb;
    }
    else {
      if (0xffdfffff < param_2 << 1) {
        return CONCAT44(param_2,param_1);
      }
      if (param_3 == 0 && (param_4 & 0x7fffffff) == 0) {
LAB_08013298:
        return CONCAT44(param_2,param_1);
      }
      if (param_2 * 2 < 0x200001 && param_2 * -2 != -0x200000) {
        bVar11 = CARRY4(param_1,param_3);
        param_1 = param_1 + param_3;
        param_2 = param_2 + (param_4 & 0x7fffffff) + (uint)bVar11;
        goto LAB_08013298;
      }
      param_4 = param_4 & 0x7fffffff;
      uVar4 = (param_2 >> 0x14) - 1;
      if (0x34 < (int)uVar4) goto LAB_08013298;
    }
    uVar8 = (int)(param_2 << 0xb | 0x80000000) >> 0xb;
    uVar10 = uVar4 - 0x20;
    if (uVar10 == 0 || (int)uVar4 < 0x20) {
      uVar5 = param_3 >> (uVar4 & 0xff);
      uVar3 = (param_3 >> (uVar4 & 0x1f) | param_3 << 0x20 - (uVar4 & 0x1f)) ^ uVar5;
      uVar10 = param_4 >> (uVar4 & 0x1f) | param_4 << 0x20 - (uVar4 & 0x1f);
      uVar6 = (uVar10 << (uVar4 & 0xff)) >> (uVar4 & 0xff);
      uVar5 = uVar5 ^ uVar10 ^ uVar6;
      uVar4 = param_1 + uVar5;
      uVar10 = uVar8 + uVar6 + (uint)CARRY4(param_1,uVar5);
      if (!CARRY4(uVar8,uVar6) && !CARRY4(uVar8 + uVar6,(uint)CARRY4(param_1,uVar5)))
      goto LAB_0801324c;
    }
    else {
      uVar5 = param_4 >> (uVar10 & 0xff);
      uVar3 = (param_4 >> (uVar10 & 0x1f) | param_4 << 0x20 - (uVar10 & 0x1f)) ^ uVar5;
      if (param_3 != 0) {
        uVar3 = uVar3 | 1;
      }
      uVar4 = param_1 + uVar5;
      uVar10 = uVar8 + CARRY4(param_1,uVar5);
      if (!CARRY4(uVar8,(uint)CARRY4(param_1,uVar5))) goto LAB_0801324c;
    }
    uVar8 = uVar10 & 1;
    uVar10 = uVar10 >> 1;
    bVar2 = (byte)uVar4;
    uVar4 = (uint)(uVar8 != 0) << 0x1f | uVar4 >> 1;
    uVar8 = uVar3 & 1;
    uVar3 = (uint)(bVar2 & 1) << 0x1f | uVar3 >> 1;
    if (uVar8 != 0) {
      uVar3 = uVar3 | 1;
    }
LAB_0801324c:
    bVar11 = 0x80000000 < uVar3 || uVar3 + 0x80000000 < (uint)((uVar4 & 1) != 0);
    uVar8 = uVar10 + 0x100000 + (param_2 & 0xfff00000) + (uint)CARRY4(uVar4,(uint)bVar11);
    if (uVar8 * 2 < 0xffe00000) {
      return CONCAT44(uVar8,uVar4 + bVar11);
    }
    return (ulonglong)(uVar8 & 0xfff00000) << 0x20;
  }
  param_4 = param_4 ^ 0x80000000;
  uVar4 = param_1 - param_3;
  if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
    uVar8 = (param_2 - param_4) - (uint)(param_3 > param_1) ^ 0x80000000;
    bVar11 = param_1 < uVar4;
    param_1 = param_1 - uVar4;
    param_2 = (param_2 - uVar8) - (uint)bVar11;
    bVar11 = CARRY4(param_3,uVar4);
    param_3 = param_3 + uVar4;
    param_4 = param_4 + uVar8 + (uint)bVar11;
  }
  uVar4 = param_2;
  if (param_2 << 1 < 0xffe00000 && 0x1fffff < param_4 * 2) {
    uVar8 = param_2 >> 0x14;
    uVar10 = uVar8 - (param_4 >> 0x14);
    if (0x36 < (int)uVar10) goto LAB_0801fcd8;
    if ((int)uVar10 < 2) {
      uVar4 = param_4 << 0xb | 0x80000000 | param_3 >> 0x15;
      if (uVar10 == 1) {
        param_3 = param_3 << 10;
        uVar4 = uVar4 >> 1;
      }
      else {
        param_3 = param_3 << 0xb;
      }
      uVar5 = param_1 >> 0x15;
      uVar10 = param_1 * 0x800;
      param_1 = uVar10 - param_3;
      uVar10 = ((param_2 << 0xb | 0x80000000 | uVar5) - uVar4) - (uint)(uVar10 < param_3);
      uVar4 = uVar8 << 0x14;
      if (-1 < (int)uVar10) {
        if (uVar10 == 0) {
          iVar9 = LZCOUNT(param_1);
          uVar10 = param_1 << iVar9;
          uVar4 = 0;
          if (uVar10 == 0) goto LAB_0801fcd8;
          param_1 = 0;
          iVar9 = iVar9 + 0x20;
        }
        else {
          iVar9 = LZCOUNT(uVar10);
          uVar10 = uVar10 << iVar9 | param_1 >> (0x20U - iVar9 & 0xff);
          param_1 = param_1 << iVar9;
        }
        uVar5 = -((int)param_2 >> 0x1f) | uVar8 << 0x15;
        uVar4 = uVar5 + iVar9 * -0x200000;
        uVar6 = uVar4 & 0xfffffffe;
        uVar8 = ((int)param_2 >> 0x1f) * -0x80000000;
        uVar4 = uVar4 >> 1 | uVar8;
        if (uVar5 < (uint)(iVar9 * 0x200000) || uVar6 == 0) {
          uVar6 = 0x200000 - uVar6 >> 0x15;
          uVar4 = uVar10 >> (uVar6 & 0x1f);
          uVar5 = uVar10 << 0x20 - (uVar6 & 0x1f);
          uVar10 = uVar10 >> (uVar6 & 0xff);
          param_1 = param_1 >> (uVar6 & 0xff) ^ (uVar4 | uVar5) ^ uVar10;
          uVar4 = uVar8;
        }
      }
      uVar8 = param_1 >> 0xb | uVar10 << 0x15;
      bVar11 = (param_1 >> 10 & 1) != 0 && (param_1 >> 0xb & 1) != 0;
      param_1 = uVar8 + bVar11;
      uVar4 = uVar4 + ((uVar10 & 0x7fffffff) >> 0xb) + (uint)CARRY4(uVar8,(uint)bVar11);
      goto LAB_0801fcd8;
    }
    uVar8 = (param_4 << 0xb | 0x80000000) >> 0xb;
LAB_0801fc6e:
    uVar5 = param_2 >> 0x14;
    uVar4 = (param_2 << 0xb | 0x80000000) >> 0xb;
    uVar6 = uVar10 - 0x20;
    if (uVar6 == 0 || (int)uVar10 < 0x20) {
      uVar6 = param_3 >> (uVar10 & 0xff);
      uVar7 = (param_3 >> (uVar10 & 0x1f) | param_3 << 0x20 - (uVar10 & 0x1f)) ^ uVar6;
      uVar3 = uVar8 >> (uVar10 & 0xff);
      uVar10 = uVar6 ^ (uVar8 >> (uVar10 & 0x1f) | uVar8 << 0x20 - (uVar10 & 0x1f)) ^ uVar3;
      uVar8 = (param_1 - uVar10) - (uint)(uVar7 != 0);
      uVar4 = (uVar4 - uVar3) - (uint)(param_1 <= uVar10 && (uint)(uVar7 == 0) <= param_1 - uVar10);
    }
    else {
      uVar10 = uVar8 >> (uVar6 & 0xff);
      uVar7 = (param_3 | param_3 << 0x10) >> 0x10 | uVar8 << (0x20 - uVar6 & 0xff);
      uVar8 = (param_1 - uVar10) - (uint)(uVar7 != 0);
      uVar4 = uVar4 - (param_1 <= uVar10 && (uint)(uVar7 == 0) <= param_1 - uVar10);
    }
    uVar10 = -uVar7;
    if (((uVar4 & 0x100000) == 0) && (uVar5 = uVar5 - 1, (uVar5 & 0x7ff) != 0)) {
      bVar1 = (uVar10 & 0x80000000) != 0;
      uVar10 = uVar7 * -2;
      uVar6 = uVar8 * 2;
      bVar11 = CARRY4(uVar8,uVar8);
      uVar8 = uVar8 * 2 + (uint)bVar1;
      uVar4 = uVar4 * 2 + (uint)(bVar11 || CARRY4(uVar6,(uint)bVar1));
    }
    bVar11 = 0x80000000 < uVar10 || uVar10 + 0x80000000 < (uint)((uVar8 & 1) != 0);
    param_1 = uVar8 + bVar11;
    uVar4 = (uVar4 & 0xffefffff) + uVar5 * 0x100000 + (uint)CARRY4(uVar8,(uint)bVar11);
  }
  else {
    if (0xffdfffff < param_2 * 2) {
      if (param_2 * 2 != 0xffe00000 || 0xffdfffff < param_4 << 1) {
        uVar4 = 0xffffffff;
      }
      goto LAB_0801fcd8;
    }
    uVar8 = param_4 & 0x7fffffff;
    if (param_3 != 0 || (param_4 & 0x7fffffff) != 0) {
      if (0x400000 < param_2 * 2 || param_2 * -2 == -0x400000) {
        uVar10 = (param_2 >> 0x14) - 1;
        if (0x34 < (int)uVar10) goto LAB_0801fcd8;
        goto LAB_0801fc6e;
      }
      bVar11 = param_1 < param_3;
      param_1 = param_1 - param_3;
      uVar4 = (param_2 - uVar8) - (uint)bVar11;
    }
    if (param_1 == 0 && (uVar4 & 0x7fffffff) == 0) {
      uVar4 = 0;
    }
  }
LAB_0801fcd8:
  return CONCAT44(uVar4,param_1);
}


// ==== __aeabi_d2iz @ 080132a0

/* library: truncates toward zero (-2.6 -> -2) */

void __aeabi_d2iz(uint param_1,uint param_2)

{
  uint uVar1;
  int iVar2;
  code *UNRECOVERED_JUMPTABLE_00;
  code *UNRECOVERED_JUMPTABLE;
  
  uVar1 = param_1 >> 0x15 | param_2 << 0xb;
  if ((param_2 & 0x80000000) == 0) {
    iVar2 = __iar_d2iz_core(uVar1,param_2 << 1);
    if (iVar2 < 0) {
      iVar2 = 0x7fffffff;
    }
                    /* WARNING: Could not recover jumptable at 0x080132b8. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    (*UNRECOVERED_JUMPTABLE_00)(iVar2);
    return;
  }
  uVar1 = __iar_d2iz_core(uVar1,param_2 << 1);
  if (uVar1 < 0x80000001) {
    iVar2 = -uVar1;
  }
  else {
    iVar2 = -0x80000000;
  }
                    /* WARNING: Could not recover jumptable at 0x080132ca. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)(iVar2);
  return;
}


// ==== __iar_d2iz_core @ 080132cc

/* library: shared core of __aeabi_d2iz: double exponent -> shifted mantissa, 0 below 1.0. library
    */

uint __iar_d2iz_core(uint param_1,uint param_2)

{
  int iVar1;
  uint uVar2;
  
  iVar1 = (param_2 >> 0x15) - 0x3ff;
  if (iVar1 < 0) {
    return 0;
  }
  uVar2 = 0x1f - iVar1;
  if ((int)uVar2 < 0) {
    uVar2 = (int)(param_1 | 0x80000000) >> 0x1f;
  }
  else {
    uVar2 = (param_1 | 0x80000000) >> (uVar2 & 0xff);
  }
  return uVar2;
}


// ==== __aeabi_fadd @ 080132ec

/* IAR runtime float add (emulated). library */

longlong __aeabi_fadd(uint param_1,uint param_2)

{
  bool bVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  bool bVar8;
  bool bVar9;
  
  if ((int)(param_1 ^ param_2) < 0) {
    param_2 = param_2 ^ 0x80000000;
    if (param_1 < param_2) {
      uVar5 = param_1 - param_2 ^ 0x80000000;
      param_1 = param_1 - uVar5;
      param_2 = param_2 + uVar5;
    }
    uVar7 = param_1 >> 0x17;
    bVar8 = 0xfeffffff < param_1 << 1;
    bVar9 = param_2 << 1 < 0x1000001;
    uVar5 = param_1;
    if (bVar8 || bVar9) {
      if (param_1 << 1 < 0xff000000) {
        if ((param_2 & 0x7fffffff) != 0) {
          uVar6 = uVar7 - (param_2 >> 0x17);
          if (1 < (int)uVar6) {
            param_2 = param_2 << 9;
            if (0x17 < (int)uVar6) {
              return CONCAT44(param_2,param_1);
            }
LAB_0801343c:
            uVar5 = param_2 >> (uVar6 & 0xff);
            if (uVar5 << (uVar6 & 0xff) < param_2) {
              uVar5 = uVar5 | 1;
            }
            uVar2 = param_1 * 0x200 - uVar5;
            if (param_1 * 0x200 < uVar5) {
              uVar2 = uVar2 * 2;
              uVar7 = uVar7 - 1;
            }
            bVar8 = (uVar2 & 0x200) != 0;
            uVar5 = uVar2 * 0x800000;
            return CONCAT44((uVar5 + 0x80000000) - (uint)!bVar8,
                            uVar7 * 0x800000 +
                            (uVar2 >> 9) +
                            (uint)(0x80000000 < uVar5 || uVar5 + 0x80000000 < (uint)bVar8));
          }
          param_2 = param_2 & 0x7fffffff;
          param_1 = param_1 - param_2;
        }
        uVar5 = param_1;
        if ((param_1 & 0x7fffffff) == 0) {
          return (ulonglong)param_2 << 0x20;
        }
      }
      else if (0xfeffffff < param_2 << 1) {
        return CONCAT44(param_2,0xffffffff);
      }
    }
    else {
      uVar2 = param_2 >> 0x17;
      bVar1 = uVar7 - uVar2 < (uint)(bVar8 || bVar9);
      uVar6 = (uVar7 - uVar2) - (uint)(!bVar8 && !bVar9);
      if (uVar2 < uVar7 || bVar1) {
        param_2 = param_2 << 8 | 0x80000000;
        if ((uVar2 < uVar7 || bVar1) && uVar6 != 0) {
          if ((int)uVar6 < 0x19) goto LAB_0801343c;
          goto LAB_08013436;
        }
        uVar5 = param_1 * 0x200 - param_2;
        if (param_2 <= param_1 * 0x200) {
          bVar8 = (uVar5 >> 8 & 1) != 0;
          if (bVar8) {
            param_2 = uVar5 >> 10;
          }
          return CONCAT44(param_2,(uVar5 >> 9) +
                                  uVar7 * 0x800000 + (uint)(bVar8 && (uVar5 >> 9 & 1) != 0));
        }
      }
      else {
        uVar5 = 0;
        if (param_1 - param_2 == 0) goto LAB_08013436;
        uVar5 = (param_1 - param_2) * 0x200;
      }
      iVar4 = LZCOUNT(uVar5) + 1;
      uVar2 = (uVar5 << iVar4) >> 9;
      iVar3 = (int)param_1 >> 0x1f;
      uVar5 = -iVar3 | uVar7 << 0x18;
      uVar7 = uVar5 + iVar4 * -0x1000000;
      if (uVar5 < (uint)(iVar4 * 0x1000000) || uVar7 == 0) {
        uVar5 = (uVar2 | 0x800000) >> (1U - ((int)uVar7 >> 0x18) & 0xff) | iVar3 * -0x80000000;
      }
      else {
        uVar5 = uVar2 + (uVar7 >> 1 | iVar3 * -0x80000000);
      }
    }
LAB_08013436:
    return CONCAT44(param_2,uVar5);
  }
  iVar3 = param_1 - param_2;
  if (param_1 < param_2) {
    param_1 = param_1 - iVar3;
    param_2 = param_2 + iVar3;
  }
  uVar7 = (param_1 & 0x7fffffff) >> 0x17;
  uVar5 = param_2;
  if (uVar7 != 0xff) {
    uVar2 = (param_2 & 0x7fffffff) >> 0x17;
    if (uVar2 == 0) {
      uVar5 = param_2 << 9;
      if (uVar5 == 0 || uVar7 == 0) {
        param_1 = param_1 + (param_2 & 0x7fffff);
        goto LAB_0801335a;
      }
      uVar2 = 0x17 - uVar7;
      if ((int)uVar2 < 0) goto LAB_0801336e;
    }
    else {
      uVar7 = uVar7 - uVar2;
      uVar2 = 0x19 - uVar7;
      if (uVar2 == 0 || 0x19 < (int)uVar7) {
LAB_0801336e:
        return CONCAT44(uVar5,param_1);
      }
      uVar5 = param_2 << 8 | 0x80000000;
    }
    uVar6 = uVar5 & ~(-0x80000000 >> (uVar2 & 0xff));
    uVar5 = uVar5 >> (uVar7 & 0xff);
    uVar7 = param_1 << 8 | 0x80000000;
    uVar2 = uVar7 + uVar5;
    if (CARRY4(uVar7,uVar5)) {
      uVar2 = uVar2 >> 1 | 0x80000000;
    }
    else {
      uVar2 = uVar2 & 0x7fffffff;
    }
    uVar6 = uVar6 | uVar2 << 0x18;
    param_1 = (param_1 & 0xff800000) +
              (uVar2 >> 8) +
              (uint)(0x80000000 < uVar6 || uVar6 + 0x80000000 < (uint)((byte)(uVar2 >> 8) & 1));
    if (param_1 * 2 < 0xff000000) {
      return CONCAT44(uVar5,param_1);
    }
    param_1 = param_1 & 0xff800000;
  }
LAB_0801335a:
  return CONCAT44(uVar5,param_1);
}


// ==== __aeabi_f2iz @ 08013370

/* float to int, truncating (emulated). library */

uint __aeabi_f2iz(uint param_1)

{
  int iVar1;
  uint uVar2;
  
  iVar1 = ((param_1 & 0x7fffffff) >> 0x17) - 0x7f;
  if (iVar1 < 0) {
    return 0;
  }
  uVar2 = 0x1f - iVar1;
  if ((int)uVar2 < 0) {
    uVar2 = 0x80000000;
    if (-1 < (int)param_1) {
      uVar2 = 0x7fffffff;
    }
    return uVar2;
  }
  if ((param_1 & 0x80000000) == 0) {
    uVar2 = (param_1 << 8 | 0x80000000) >> (uVar2 & 0xff);
    if ((int)uVar2 < 0) {
      uVar2 = 0x7fffffff;
    }
    return uVar2;
  }
  uVar2 = (param_1 << 8 | 0x80000000) >> (uVar2 & 0xff);
  if ((int)uVar2 < 0) {
    uVar2 = 0x80000000;
  }
  else {
    uVar2 = -uVar2;
  }
  return uVar2;
}


// ==== __aeabi_fsub @ 080133b4

/* library: float a - b (flips b's sign, shares 0x080132fc add path of __aeabi_fadd); emu_call 5-2 =
   3, -1.5-2 = -3.5. high (emulated) */

longlong __aeabi_fsub(uint param_1,uint param_2)

{
  bool bVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  int iVar5;
  uint uVar6;
  uint uVar7;
  bool bVar8;
  bool bVar9;
  
  if ((int)(param_1 ^ param_2) < 0) {
    param_2 = param_2 ^ 0x80000000;
    iVar3 = param_1 - param_2;
    if (param_1 < param_2) {
      param_1 = param_1 - iVar3;
      param_2 = param_2 + iVar3;
    }
    uVar4 = (param_1 & 0x7fffffff) >> 0x17;
    uVar2 = param_2;
    if (uVar4 != 0xff) {
      uVar6 = (param_2 & 0x7fffffff) >> 0x17;
      if (uVar6 == 0) {
        uVar2 = param_2 << 9;
        if (uVar2 == 0 || uVar4 == 0) {
          param_1 = param_1 + (param_2 & 0x7fffff);
          goto LAB_0801335a;
        }
        uVar6 = 0x17 - uVar4;
        if ((int)uVar6 < 0) goto LAB_0801336e;
      }
      else {
        uVar4 = uVar4 - uVar6;
        uVar6 = 0x19 - uVar4;
        if (uVar6 == 0 || 0x19 < (int)uVar4) {
LAB_0801336e:
          return CONCAT44(uVar2,param_1);
        }
        uVar2 = param_2 << 8 | 0x80000000;
      }
      uVar7 = uVar2 & ~(-0x80000000 >> (uVar6 & 0xff));
      uVar2 = uVar2 >> (uVar4 & 0xff);
      uVar4 = param_1 << 8 | 0x80000000;
      uVar6 = uVar4 + uVar2;
      if (CARRY4(uVar4,uVar2)) {
        uVar6 = uVar6 >> 1 | 0x80000000;
      }
      else {
        uVar6 = uVar6 & 0x7fffffff;
      }
      uVar7 = uVar7 | uVar6 << 0x18;
      param_1 = (param_1 & 0xff800000) +
                (uVar6 >> 8) +
                (uint)(0x80000000 < uVar7 || uVar7 + 0x80000000 < (uint)((byte)(uVar6 >> 8) & 1));
      if (param_1 * 2 < 0xff000000) {
        return CONCAT44(uVar2,param_1);
      }
      param_1 = param_1 & 0xff800000;
    }
LAB_0801335a:
    return CONCAT44(uVar2,param_1);
  }
  if (param_1 < param_2) {
    uVar2 = param_1 - param_2 ^ 0x80000000;
    param_1 = param_1 - uVar2;
    param_2 = param_2 + uVar2;
  }
  uVar4 = param_1 >> 0x17;
  bVar8 = 0xfeffffff < param_1 << 1;
  bVar9 = param_2 << 1 < 0x1000001;
  uVar2 = param_1;
  if (bVar8 || bVar9) {
    if (param_1 << 1 < 0xff000000) {
      if ((param_2 & 0x7fffffff) != 0) {
        uVar7 = uVar4 - (param_2 >> 0x17);
        if (1 < (int)uVar7) {
          param_2 = param_2 << 9;
          if (0x17 < (int)uVar7) {
            return CONCAT44(param_2,param_1);
          }
LAB_0801343c:
          uVar2 = param_2 >> (uVar7 & 0xff);
          if (uVar2 << (uVar7 & 0xff) < param_2) {
            uVar2 = uVar2 | 1;
          }
          uVar6 = param_1 * 0x200 - uVar2;
          if (param_1 * 0x200 < uVar2) {
            uVar6 = uVar6 * 2;
            uVar4 = uVar4 - 1;
          }
          bVar8 = (uVar6 & 0x200) != 0;
          uVar2 = uVar6 * 0x800000;
          return CONCAT44((uVar2 + 0x80000000) - (uint)!bVar8,
                          uVar4 * 0x800000 +
                          (uVar6 >> 9) +
                          (uint)(0x80000000 < uVar2 || uVar2 + 0x80000000 < (uint)bVar8));
        }
        param_2 = param_2 & 0x7fffffff;
        param_1 = param_1 - param_2;
      }
      uVar2 = param_1;
      if ((param_1 & 0x7fffffff) == 0) {
        return (ulonglong)param_2 << 0x20;
      }
    }
    else if (0xfeffffff < param_2 << 1) {
      return CONCAT44(param_2,0xffffffff);
    }
  }
  else {
    uVar6 = param_2 >> 0x17;
    bVar1 = uVar4 - uVar6 < (uint)(bVar8 || bVar9);
    uVar7 = (uVar4 - uVar6) - (uint)(!bVar8 && !bVar9);
    if (uVar6 < uVar4 || bVar1) {
      param_2 = param_2 << 8 | 0x80000000;
      if ((uVar6 < uVar4 || bVar1) && uVar7 != 0) {
        if ((int)uVar7 < 0x19) goto LAB_0801343c;
        goto LAB_08013436;
      }
      uVar2 = param_1 * 0x200 - param_2;
      if (param_2 <= param_1 * 0x200) {
        bVar8 = (uVar2 >> 8 & 1) != 0;
        if (bVar8) {
          param_2 = uVar2 >> 10;
        }
        return CONCAT44(param_2,(uVar2 >> 9) +
                                uVar4 * 0x800000 + (uint)(bVar8 && (uVar2 >> 9 & 1) != 0));
      }
    }
    else {
      uVar2 = 0;
      if (param_1 - param_2 == 0) goto LAB_08013436;
      uVar2 = (param_1 - param_2) * 0x200;
    }
    iVar5 = LZCOUNT(uVar2) + 1;
    uVar6 = (uVar2 << iVar5) >> 9;
    iVar3 = (int)param_1 >> 0x1f;
    uVar2 = -iVar3 | uVar4 << 0x18;
    uVar4 = uVar2 + iVar5 * -0x1000000;
    if (uVar2 < (uint)(iVar5 * 0x1000000) || uVar4 == 0) {
      uVar2 = (uVar6 | 0x800000) >> (1U - ((int)uVar4 >> 0x18) & 0xff) | iVar3 * -0x80000000;
    }
    else {
      uVar2 = uVar6 + (uVar4 >> 1 | iVar3 * -0x80000000);
    }
  }
LAB_08013436:
  return CONCAT44(param_2,uVar2);
}


// ==== Analog_PollAlternate @ 0801349c

/* main: toggles 0x20008086 and runs Thermistor_Update or Battery_Update on alternate calls. high */

undefined4 Analog_PollAlternate(void)

{
  undefined4 unaff_r7;
  
  DAT_20008086 = DAT_20008086 + 1;
  if (1 < DAT_20008086) {
    DAT_20008086 = 0;
  }
  if (DAT_20008086 == 0) {
    Thermistor_Update();
  }
  else if (DAT_20008086 == 1) {
    Battery_Update();
  }
  return unaff_r7;
}


// ==== Battery_Update @ 080134da

/* PA5 -> linear table (10.50..3.00 V) -> 20-sample trimmed mean -> 0.1 V; low under 4.1 V after 10
   readings. high */

void Battery_Update(void)

{
  undefined4 uVar1;
  uint uVar2;
  ushort uVar3;
  
  DAT_20007fd2 = ADC_ReadOnce(1);
  ADC_Off();
  uVar3 = DAT_20007fd2;
  if (DAT_20007fd2 < 0x3fc) {
    uVar3 = 0x3fc;
  }
  if (0xc30 < DAT_20007fd2) {
    uVar3 = 0xc30;
  }
  uVar1 = Battery_TableToCentivolts(uVar3);
  uVar2 = TrimmedMean20(uVar1);
  g_battery_dV = (ushort)(uVar2 / 10);
  g_low_battery = g_battery_dV < 0x29;
  if (g_setup_page == '\x01') {
    DAT_20008062 = 1;
  }
  if (g_low_battery == DAT_2000803b) {
    DAT_20007fdc = 0;
  }
  else {
    DAT_20007fdc = DAT_20007fdc + 1;
    if (10 < DAT_20007fdc) {
      DAT_20007fdc = 0;
      DAT_2000803b = g_low_battery;
      LCD_DrawLowBatIcon(1);
    }
  }
  return;
}


// ==== TrimmedMean20 @ 080135b0

/* last 20 values, sorted, drop 5 high + 5 low, mean of 10. high */

undefined8
TrimmedMean20(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  uint uVar2;
  
  uVar2 = 0;
  if (DAT_20007fbc == 0x14) {
    for (uVar1 = 1; uVar1 < DAT_20007fbc; uVar1 = uVar1 + 1) {
      (&DAT_200076f0)[DAT_20007fbc - uVar1] = (&DAT_200076f0)[(DAT_20007fbc - 1) - uVar1];
    }
    DAT_200076f0 = param_1;
    TrimmedMean20_Sort();
    for (uVar1 = 5; uVar1 < DAT_20007fbc - 5; uVar1 = uVar1 + 1) {
      uVar2 = (&DAT_20007740)[uVar1] + uVar2;
    }
    uVar2 = uVar2 / (DAT_20007fbc - 10);
  }
  else {
    (&DAT_200076f0)[DAT_20007fbc] = param_1;
    for (uVar1 = 0; uVar1 <= DAT_20007fbc; uVar1 = uVar1 + 1) {
      uVar2 = (&DAT_200076f0)[uVar1] + uVar2;
    }
    uVar2 = uVar2 / (DAT_20007fbc + 1);
  }
  DAT_20007fbc = DAT_20007fbc + 1;
  if (0x14 < DAT_20007fbc) {
    DAT_20007fbc = 0x14;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== TrimmedMean20_Sort @ 0801368a

/* TrimmedMean20 helper: copies the 20 samples 0x200076f0 to 0x20007740 and selection-sorts them
   descending in place (repeated max, zeroing the picked entry; unsigned compare). high */

void TrimmedMean20_Sort(void)

{
  uint uVar1;
  uint uVar2;
  byte bVar3;
  uint uVar4;
  uint local_58 [20];
  
  uVar2 = 0;
  for (uVar4 = 0; (uVar4 & 0xff) < 0x14; uVar4 = (uVar4 & 0xff) + 1) {
    (&DAT_20007740)[uVar4 & 0xff] = (&DAT_200076f0)[uVar4 & 0xff];
  }
  for (uVar4 = 0; (uVar4 & 0xff) < 0x14; uVar4 = (uVar4 & 0xff) + 1) {
    uVar1 = 0;
    for (bVar3 = 0; bVar3 < 0x14; bVar3 = bVar3 + 1) {
      if (uVar1 <= (uint)(&DAT_20007740)[bVar3]) {
        uVar2 = (uint)bVar3;
        uVar1 = (&DAT_20007740)[uVar2];
      }
    }
    (&DAT_20007740)[uVar2] = 0;
    local_58[uVar4 & 0xff] = uVar1;
  }
  for (uVar2 = 0; (uVar2 & 0xff) < 0x14; uVar2 = (uVar2 & 0xff) + 1) {
    (&DAT_20007740)[uVar2 & 0xff] = local_58[uVar2 & 0xff];
  }
  return;
}


// ==== Battery_TableToCentivolts @ 08013728

/* 16-entry linear table at 0x08028c9c: raw 0xC30..0x3FC -> 1050..300 (10 mV). high */

ulonglong Battery_TableToCentivolts
                    (uint param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  bool bVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  int iVar4;
  uint uVar5;
  
  bVar1 = true;
  uVar5 = 0;
  while (bVar1) {
    uVar5 = uVar5 & 0xff;
    if (param_1 < (uint)(&DAT_08028c9c)[uVar5]) {
      uVar5 = uVar5 + 1;
    }
    else {
      bVar1 = false;
    }
  }
  uVar2 = __aeabi_ui2f((param_1 - (&DAT_08028c9c)[uVar5 & 0xff]) * 0x32);
  uVar3 = __aeabi_ui2f((&DAT_08028c98)[uVar5 & 0xff] - (&DAT_08028c9c)[uVar5 & 0xff]);
  __aeabi_fdiv(uVar2,uVar3);
  iVar4 = __aeabi_f2iz();
  return CONCAT44(param_4,(uVar5 & 0xff) * -0x32 + 0x41a + iVar4) & 0xffffffff0000ffff;
}


// ==== ADC_ReadOnce @ 080137b4  [ADC_SR]

/* PA5 (arg 1, battery) or PA4 (arg 0, thermistor), 384-cycle sample, ADC on then off. high */

void ADC_ReadOnce(char param_1)

{
  int iVar1;
  
  RCC_APB2PeriphClockCmd(0x200,1);
  ADC_Cmd(&ADC_SR,1);
  do {
    iVar1 = ADC_GetFlagStatus(&ADC_SR,0x40);
  } while (iVar1 == 0);
  if (param_1 == '\x01') {
    ADC_RegularChannelConfig(&ADC_SR,5,1,7);
  }
  else {
    ADC_RegularChannelConfig(&ADC_SR,4,1,7);
  }
  ADC_SoftwareStartConv(&ADC_SR);
  do {
    iVar1 = ADC_GetFlagStatus(&ADC_SR,2);
  } while (iVar1 == 0);
  ADC_GetConversionValue(&ADC_SR);
  return;
}


// ==== Thermistor_Update @ 0801381e

/* PA4 -> 0801385a. high */

undefined4 Thermistor_Update(void)

{
  undefined4 unaff_r7;
  
  DAT_20007fd2 = ADC_ReadOnce(0);
  ADC_Off();
  if ((DAT_20007fd2 < 0x8000) && (Thermistor_Process(DAT_20007fd2), g_setup_page == '\0')) {
    DAT_20008062 = 1;
  }
  return unaff_r7;
}


// ==== Thermistor_Process @ 0801385a

/* (raw PA4) < 51 counts 10 times -> thermistor fault 0x200080ac (cold junction 0x20007e7c = 0);
   else 10-sample moving mean 0x20007fd0 -> 08013aac (0.01 deg C) + trim 0x20007fbe; cal mode MEM
   (0x20008096): trim = 2300 (23.00 C) - value -> EEPROM 0x2e0 (Cal_StoreField 200); shown /10 via
   08013cf8 unless 0x200080af. high */

undefined4
Thermistor_Process(ushort param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  ushort uVar2;
  bool bVar3;
  
  bVar3 = false;
  uVar2 = 0;
  DAT_20007fce = param_1;
  if (param_1 < 0x33) {
    DAT_20007fd4 = 0;
    DAT_20007fd6 = DAT_20007fd6 + 1;
    if (10 < DAT_20007fd6) {
      DAT_20007fd6 = 10;
      DAT_200080ac = '\x01';
    }
    bVar3 = true;
  }
  else {
    DAT_20007fd6 = 0;
    DAT_200080ac = '\0';
    if (DAT_20007fd4 == 10) {
      for (uVar1 = 1; (uVar1 & 0xffff) < (uint)DAT_20007fd4; uVar1 = (uVar1 & 0xffff) + 1) {
        (&DAT_20007ba8)[(uint)DAT_20007fd4 - (uVar1 & 0xffff)] =
             (&DAT_20007ba8)[(DAT_20007fd4 - 1) - (uVar1 & 0xffff)];
      }
      DAT_20007ba8 = DAT_20007fce;
      for (uVar1 = 0; (uVar1 & 0xffff) < (uint)DAT_20007fd4; uVar1 = (uVar1 & 0xffff) + 1) {
        uVar2 = (&DAT_20007ba8)[uVar1 & 0xffff] + uVar2;
      }
      DAT_20007fd0 = uVar2 / DAT_20007fd4;
    }
    else {
      (&DAT_20007ba8)[DAT_20007fd4] = param_1;
      for (uVar1 = 0; (uVar1 & 0xffff) <= (uint)DAT_20007fd4; uVar1 = (uVar1 & 0xffff) + 1) {
        uVar2 = (&DAT_20007ba8)[uVar1 & 0xffff] + uVar2;
      }
      DAT_20007fd0 = (ushort)((uint)uVar2 / (DAT_20007fd4 + 1));
    }
    DAT_20007fd4 = DAT_20007fd4 + 1;
    if (10 < DAT_20007fd4) {
      DAT_20007fd4 = 10;
    }
  }
  if (!bVar3) {
    DAT_20007fcc = Thermistor_AdcToTemp(DAT_20007fd0);
    if ((g_cal_mode != '\0') && (DAT_20008096 != '\0')) {
      DAT_20008096 = '\0';
      DAT_20008097 = 3;
      DAT_20007fbe = 0x8fc - DAT_20007fcc;
      Cal_StoreField(200,(int)DAT_20007fbe);
    }
    DAT_20007fcc = DAT_20007fbe + DAT_20007fcc;
    if (DAT_200080af == '\0') {
      DAT_200080b0 = 0;
      bVar3 = -1 < (int)((uint)DAT_20007fcc << 0x10);
      if (bVar3) {
        uVar1 = (uint)(short)DAT_20007fcc;
      }
      else {
        uVar1 = ~(int)(short)DAT_20007fcc;
      }
      TC_ColdJunctionEmf(!bVar3,(int)uVar1 / 10);
    }
    else {
      DAT_200080b0 = DAT_200080b0 + 1;
      if (10 < DAT_200080b0) {
        DAT_200080b0 = 10;
        DAT_200080ac = '\x01';
      }
    }
  }
  if (DAT_200080ac == '\x01') {
    DAT_20008062 = 1;
    DAT_20007e7c = 0;
  }
  return param_4;
}


// ==== Thermistor_AdcToTemp @ 08013aac

/* Thermistor_Process: internal thermistor ADC count -> temperature in 0.01 degC by table 0x08028dd0
   (5 degC steps, linear interpolation): 0x206 -> 85.00, 0xE58 -> -25.00 (count falls as temp
   rises); outside -> 0 with error flag 0x200080af = 1. high (emulated) */

undefined8
Thermistor_AdcToTemp(ushort param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  bool bVar1;
  short sVar2;
  undefined4 uVar3;
  undefined4 uVar4;
  uint uVar5;
  uint uVar6;
  
  bVar1 = true;
  DAT_200080ae = 0;
  if ((param_1 < 0x206) || (uVar5 = (uint)param_1, 0xe58 < uVar5)) {
    sVar2 = 0;
    DAT_200080af = 1;
  }
  else {
    uVar6 = 0;
    while (bVar1) {
      uVar6 = uVar6 & 0xff;
      if (uVar5 < (ushort)(&DAT_08028dd0)[uVar6]) {
        uVar6 = uVar6 + 1;
      }
      else {
        bVar1 = false;
      }
    }
    uVar3 = __aeabi_i2f((*(ushort *)(&UNK_08028dce + (uVar6 & 0xff) * 2) - uVar5) * 500);
    uVar4 = __aeabi_i2f((uint)*(ushort *)(&UNK_08028dce + (uVar6 & 0xff) * 2) -
                        (uint)(ushort)(&DAT_08028dd0)[uVar6 & 0xff]);
    __aeabi_fdiv(uVar3,uVar4);
    sVar2 = __aeabi_f2iz();
    sVar2 = ((ushort)uVar6 & 0xff) * 500 + -0xdac + sVar2;
    DAT_200080af = 0;
  }
  return CONCAT44(param_4,(int)sVar2);
}


// ==== Temp_CToF @ 08013b76

/* (0.01 degC, negatives as ones' complement ~x) -> 0.01 degF: x * 1.8 + 3200, rounded, same sign
   encoding; also result in 0x20007e88. Mode_Temp_OnAD1 (deg F setting) and 08018ad8 (sub
   temperature). high (emulated: 2500 -> 7700, 0 -> 3200, -10.01 -> 1398) */

undefined8 Temp_CToF(uint param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  bool bVar4;
  undefined1 uVar5;
  bool bVar6;
  undefined8 uVar7;
  
  bVar6 = -1 < (int)(param_1 << 0x10);
  if (bVar6) {
    DAT_20007e88 = param_1;
  }
  else {
    DAT_20007e88 = ~param_1;
  }
  __aeabi_i2f(DAT_20007e88);
  uVar7 = __aeabi_f2d();
  __aeabi_dmul((int)uVar7,(int)((ulonglong)uVar7 >> 0x20),0xcccccccd,0x3ffccccc);
  iVar1 = __aeabi_d2f();
  uVar5 = true;
  if (bVar6) {
    uVar2 = __aeabi_fadd(0x45480000,iVar1);
    bVar6 = false;
  }
  else {
    bVar6 = iVar1 == 0;
    __aeabi_cfrcmple(iVar1,0x45480001);
    if ((bool)uVar5 && !bVar6) {
      bVar6 = false;
      uVar2 = __aeabi_fsub(0x45480000,iVar1);
    }
    else {
      bVar6 = true;
      uVar2 = __aeabi_fadd(0xc5480000,iVar1);
    }
  }
  DAT_20007e88 = __aeabi_f2iz(uVar2);
  uVar3 = __aeabi_i2f(DAT_20007e88);
  uVar2 = __aeabi_fsub(uVar2,uVar3);
  bVar4 = false;
  __aeabi_cfrcmple(uVar2);
  if (!(bool)uVar5 || bVar4) {
    DAT_20007e88 = DAT_20007e88 + 1;
  }
  if (bVar6) {
    DAT_20007e88 = ~DAT_20007e88;
  }
  return CONCAT44(param_4,DAT_20007e88);
}


// ==== TC_EmfToTemp @ 08013c40

/* Mode_Temp_OnAD1: K thermocouple EMF in uV (reading + cold-junction EMF) -> 0x20007e80 in 0.01
   degC by the K table 0x08025cf8 (10 degC steps, linear interpolation); valid -5891..54138 uV =
   -200..1350 degC; below -> 0, flag 0x200080ad = 2; above -> 0, flag 1. high (emulated: 41276 ->
   100000, -5891 -> -20000) */

void TC_EmfToTemp(int param_1)

{
  bool bVar1;
  int iVar2;
  uint uVar3;
  
  bVar1 = true;
  DAT_200080ad = 0;
  if (param_1 < -0x1703) {
    DAT_20007e80 = 0;
    DAT_200080ad = 2;
  }
  else if (param_1 < 0xd37b) {
    uVar3 = 0;
    while (bVar1) {
      uVar3 = uVar3 & 0xff;
      if ((int)(&DAT_08025cf8)[uVar3] < param_1) {
        uVar3 = uVar3 + 1;
      }
      else {
        bVar1 = false;
      }
    }
    DAT_20007e84 = (uVar3 & 0xff) * 1000 + -22000;
    __aeabi_i2f(((*(int *)(&DAT_08025cf4 + (uVar3 & 0xff) * 4) - param_1) * 1000) /
                (*(int *)(&DAT_08025cf4 + (uVar3 & 0xff) * 4) - (&DAT_08025cf8)[uVar3 & 0xff]));
    iVar2 = __aeabi_f2iz();
    DAT_20007e80 = DAT_20007e84 + iVar2;
    DAT_20007e88 = DAT_20007e80;
  }
  else {
    DAT_20007e80 = 0;
    DAT_200080ad = 1;
  }
  return;
}


// ==== TC_ColdJunctionEmf @ 08013cf8

/* (negative, |temp| in 0.1 degC) from Thermistor_Process: cold-junction temperature -> K EMF uV in
   0x20007e7c by the same table (0 degC = index 21); valid 0..100.0 degC or down to -20.0 degC; out
   of range leaves 0x20007e7c unchanged (stale). high (emulated: 25.0 -> 998, -10.0 -> -392) */

void TC_ColdJunctionEmf(char param_1,int param_2)

{
  uint uVar1;
  
  if (((param_1 == '\0') || (param_2 < 0xc9)) && (param_2 < 0x3e9)) {
    if (param_1 == '\0') {
      uVar1 = param_2 / 100 + 0x15;
      DAT_20007e7c = ((param_2 % 100) *
                     ((((&DAT_08025cfc)[uVar1 & 0xff] - (&DAT_08025cf8)[uVar1 & 0xff]) * 10) / 100))
                     / 10 + (&DAT_08025cf8)[uVar1 & 0xff];
    }
    else {
      uVar1 = 0x15 - param_2 / 100;
      DAT_20007e7c = (&DAT_08025cf8)[uVar1 & 0xff] -
                     ((param_2 % 100) *
                     ((int)((~*(uint *)(&DAT_08025cf4 + (uVar1 & 0xff) * 4) -
                            ~(&DAT_08025cf8)[uVar1 & 0xff]) * 10) / 100)) / 10;
    }
  }
  return;
}


// ==== Board_ADCInit @ 08013dd4  [ADC_SR]

/* ADC on, single conversion; waits ADONS with NO timeout. high */

void Board_ADCInit(void)

{
  int iVar1;
  undefined4 local_20;
  undefined1 local_1c;
  undefined1 local_1b;
  undefined4 local_14;
  undefined4 local_10;
  undefined1 local_c;
  
  ADC_DeInit(&ADC_SR);
  local_20 = 0;
  local_1c = 0;
  local_1b = 0;
  local_14 = 0;
  local_10 = 0;
  local_c = 1;
  ADC_Init(&ADC_SR,&local_20);
  ADC_Cmd(&ADC_SR,1);
  do {
    iVar1 = ADC_GetFlagStatus(&ADC_SR,0x40);
  } while (iVar1 == 0);
  return;
}


// ==== __aeabi_i2d @ 08013ec4

/* library: int -> double (negates, ui2d core, sets sign). library */

undefined8 __aeabi_i2d(int param_1)

{
  uint uVar1;
  int iVar2;
  int iVar3;
  code *UNRECOVERED_JUMPTABLE;
  undefined8 uVar4;
  
  if (param_1 < 0) {
    uVar4 = __iar_ui2d_core(-param_1);
                    /* WARNING: Could not recover jumptable at 0x08013ed6. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar4 = (*UNRECOVERED_JUMPTABLE)((int)uVar4,(uint)((ulonglong)uVar4 >> 0x20) | 0x80000000);
    return uVar4;
  }
  iVar2 = 0;
  iVar3 = 0;
  if (param_1 != 0) {
    uVar1 = param_1 << LZCOUNT(param_1);
    iVar3 = (0x420 - (LZCOUNT(param_1) + 3)) * 0x100000 + (uVar1 >> 0xb);
    iVar2 = uVar1 << 0x15;
  }
  return CONCAT44(iVar3,iVar2);
}


// ==== __aeabi_ui2d @ 08013ed8

/* library: unsigned int -> double; HY_RmsRead, ACDC_Combine. library */

undefined8 __aeabi_ui2d(int param_1)

{
  uint uVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = 0;
  iVar3 = 0;
  if (param_1 != 0) {
    uVar1 = param_1 << LZCOUNT(param_1);
    iVar3 = (0x420 - (LZCOUNT(param_1) + 3)) * 0x100000 + (uVar1 >> 0xb);
    iVar2 = uVar1 << 0x15;
  }
  return CONCAT44(iVar3,iVar2);
}


// ==== __iar_ui2d_core @ 08013edc

/* library: interior of __aeabi_ui2d past its zero test; called by __aeabi_i2d. library */

undefined8 __iar_ui2d_core(int param_1,int param_2)

{
  uint uVar1;
  char in_ZR;
  
  if (in_ZR == '\0') {
    uVar1 = param_1 << LZCOUNT(param_1);
    param_2 = (0x420 - (LZCOUNT(param_1) + 3)) * 0x100000 + (uVar1 >> 0xb);
    param_1 = uVar1 << 0x15;
  }
  return CONCAT44(param_2,param_1);
}


// ==== sqrt @ 08013ef4

/* library: double sqrt (IAR), errno EDOM (0x21) for negatives; emu_call 16 -> 4, 3 -> 1.732, -4 ->
   NaN. Callers 0800827c (VA), 0800a6a8. high (emulated) */

undefined8 sqrt(uint param_1,uint param_2)

{
  uint uVar1;
  undefined4 *puVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  bool bVar10;
  bool bVar11;
  
  uVar6 = param_2 >> 0x14;
  if (uVar6 == 0) {
    if (param_1 == 0 && param_2 == 0) goto LAB_08013f96;
    iVar4 = LZCOUNT(param_2);
    if (param_2 == 0) {
      iVar4 = LZCOUNT(param_1) + 0x20;
    }
    uVar6 = iVar4 - 0xb;
    if (uVar6 < 0x20) {
      uVar3 = param_2 << (uVar6 & 0xff) | param_1 >> (0x20 - uVar6 & 0xff);
    }
    else {
      uVar3 = param_1 << (iVar4 - 0x2bU & 0xff);
    }
    param_1 = param_1 << (uVar6 & 0xff);
    uVar6 = 1 - uVar6;
  }
  else {
    if (0x7fe < uVar6) {
      if (((uVar6 != 0x7ff) && (param_2 < 0xfff00001)) &&
         (uVar3 = param_2 & 0x7fffffff, param_2 = 0, param_1 != 0 || uVar3 != 0)) {
        puVar2 = (undefined4 *)__aeabi_errno_addr();
        *puVar2 = 0x21;
        return CONCAT44(~uVar6,param_1);
      }
      goto LAB_08013f96;
    }
    uVar3 = param_2 & ~(uVar6 << 0x14) | 0x100000;
  }
  if ((uVar6 + 0x3fd & 1) != 0) {
    bVar10 = CARRY4(param_1,param_1);
    param_1 = param_1 * 2;
    uVar3 = uVar3 * 2 + (uint)bVar10;
  }
  uVar1 = param_1 << 2;
  uVar8 = 0x300000;
  uVar7 = 0;
  uVar5 = 0x80000000;
  iVar4 = (uVar3 << 2 | param_1 >> 0x1e) - 0x900000;
  uVar3 = 0x100000;
  while( true ) {
    for (; -1 < iVar4; iVar4 = (iVar4 * 2 + (uint)bVar10) - uVar9) {
      uVar9 = uVar3 >> 1;
      if ((uVar3 & 1) != 0) goto LAB_08013f70;
      iVar4 = iVar4 - uVar8;
      uVar8 = uVar8 + uVar9;
      bVar10 = CARRY4(uVar1,uVar1);
      uVar1 = uVar1 * 2;
      uVar3 = uVar9;
    }
    uVar9 = uVar3 >> 1;
    if ((uVar3 & 1) != 0) break;
    iVar4 = iVar4 + uVar8;
    uVar8 = uVar8 - uVar9;
    bVar10 = CARRY4(uVar1,uVar1);
    uVar1 = uVar1 * 2;
    iVar4 = (iVar4 * 2 + (uint)bVar10) - uVar9;
    uVar3 = uVar9;
  }
  do {
    bVar10 = CARRY4(uVar1,uVar7);
    uVar9 = uVar1 + uVar7;
    iVar4 = iVar4 + uVar8;
    bVar11 = uVar7 < uVar5;
    uVar7 = uVar7 - uVar5;
    uVar8 = (uVar8 - (uVar3 >> 1)) - (uint)bVar11;
    uVar1 = uVar9 * 2 - uVar5;
    for (iVar4 = (((iVar4 + (uint)bVar10) * 2 + (uint)CARRY4(uVar9,uVar9)) - (uVar3 >> 1)) -
                 (uint)(uVar9 * 2 < uVar5); -1 < iVar4;
        iVar4 = (((iVar4 - (uint)bVar10) * 2 + (uint)CARRY4(uVar9,uVar9)) - (uVar3 >> 1)) -
                (uint)(uVar9 * 2 < uVar5)) {
      uVar5 = uVar5 >> 1;
      if (uVar5 == 0) {
        bVar10 = 0xfffffffe < uVar7;
        uVar7 = uVar7 + 1;
        uVar8 = uVar8 + (uVar3 >> 1) + (uint)bVar10;
        goto LAB_08013f8a;
      }
LAB_08013f70:
      bVar10 = uVar1 < uVar7;
      uVar9 = uVar1 - uVar7;
      iVar4 = iVar4 - uVar8;
      bVar11 = CARRY4(uVar7,uVar5);
      uVar7 = uVar7 + uVar5;
      uVar8 = uVar8 + (uVar3 >> 1) + (uint)bVar11;
      uVar1 = uVar9 * 2 - uVar5;
    }
    uVar5 = uVar5 >> 1;
  } while (uVar5 != 0);
LAB_08013f8a:
  param_1 = (uint)((uVar8 & 1) != 0) << 0x1f | uVar7 >> 1;
  param_2 = (uVar8 >> 1) + (uVar6 + 0x3fd >> 1) * 0x100000;
LAB_08013f96:
  return CONCAT44(param_2,param_1);
}


// ==== __aeabi_ldivmod @ 08013fe4

/* 64-bit signed divide (emulated). library */

uint __aeabi_ldivmod(uint param_1,uint param_2,uint param_3,uint param_4)

{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  bool bVar5;
  bool bVar6;
  bool bVar7;
  
  uVar4 = param_4 & 0x80000000;
  if ((int)uVar4 < 0) {
    bVar5 = param_3 != 0;
    param_3 = -param_3;
    param_4 = -(uint)bVar5 - param_4;
  }
  uVar4 = uVar4 ^ (int)param_2 >> 0x20;
  if (uVar4 != 0) {
    if ((int)param_2 < 0) {
      bVar5 = param_1 != 0;
      param_1 = -param_1;
      param_2 = -(uint)bVar5 - param_2;
    }
    uVar2 = __aeabi_uldivmod(param_1,param_2);
    if ((uVar4 & 0x80000000) != 0) {
      uVar2 = -uVar2;
    }
    return uVar2;
  }
  if (param_4 != 0) {
    if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
      return 0;
    }
    uVar4 = LZCOUNT(param_4) - LZCOUNT(param_2);
    param_4 = param_4 << (uVar4 & 0xff);
LAB_0801406a:
    param_4 = param_4 | param_3 >> (0x20 - uVar4 & 0xff);
    param_3 = param_3 << (uVar4 & 0xff);
    uVar4 = 0x80000000 >> (uVar4 & 0xff);
    while( true ) {
      bVar5 = param_2 - param_4 < (uint)(param_3 <= param_1);
      bVar6 = param_4 < param_2 || bVar5;
      if (param_4 < param_2 || bVar5) {
        param_2 = (param_2 - param_4) - (uint)(param_3 > param_1);
        param_1 = param_1 - param_3;
      }
      uVar2 = uVar4 * 2;
      bVar5 = CARRY4(uVar4,uVar4);
      uVar4 = uVar4 * 2 + (uint)bVar6;
      if (bVar5 || CARRY4(uVar2,(uint)bVar6)) break;
      uVar2 = param_4 & 1;
      param_4 = param_4 >> 1;
      param_3 = (uint)(uVar2 != 0) << 0x1f | param_3 >> 1;
    }
    return uVar4;
  }
  if (param_2 == 0) {
    if (param_3 != 0) {
      return param_1 / param_3;
    }
  }
  else {
    if (1 < param_3) {
      if (param_3 >> 0x10 == 0) {
        uVar2 = (param_2 - param_3 * (param_2 / param_3)) * 0x10000 | param_1 >> 0x10;
        uVar4 = uVar2 / param_3;
        return (param_1 & 0xffff | (uVar2 - param_3 * uVar4) * 0x10000) / param_3 | uVar4 << 0x10;
      }
      uVar4 = (0x20 - LZCOUNT(param_2)) + LZCOUNT(param_3);
      param_4 = 0;
      if (0x1f < uVar4) {
        param_3 = param_3 << (uVar4 - 0x20 & 0xff);
        uVar2 = 0x80000000 >> (uVar4 - 0x20 & 0xff);
        uVar3 = 0;
        uVar4 = 0;
        while( true ) {
          bVar5 = param_2 - param_3 < (uint)(uVar4 <= param_1);
          bVar6 = param_3 < param_2 || bVar5;
          if (param_3 < param_2 || bVar5) {
            param_2 = (param_2 - param_3) - (uint)(uVar4 > param_1);
            param_1 = param_1 - uVar4;
          }
          bVar7 = CARRY4(uVar2,uVar2) || CARRY4(uVar2 * 2,(uint)bVar6);
          uVar2 = uVar2 * 2 + (uint)bVar6;
          uVar1 = uVar3 * 2;
          bVar5 = CARRY4(uVar3,uVar3);
          uVar3 = uVar3 * 2 + (uint)bVar7;
          if (bVar5 || CARRY4(uVar1,(uint)bVar7)) break;
          uVar1 = param_3 & 1;
          param_3 = param_3 >> 1;
          uVar4 = (uint)(uVar1 != 0) << 0x1f | uVar4 >> 1;
        }
        return uVar2;
      }
      goto LAB_0801406a;
    }
    if (param_3 != 0) {
      return param_1;
    }
  }
  return param_1;
}


// ==== __aeabi_uldivmod @ 08014028

/* library (IAR/AEABI): u64 r0:r1 / r2:r3, quotient r0:r1 (remainder r2:r3); 1000000007/10 ->
   100000000. high (emulated) */

ulonglong __aeabi_uldivmod(uint param_1,uint param_2,uint param_3,uint param_4)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  bool bVar6;
  bool bVar7;
  
  if (param_4 != 0) {
    if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
      return 0;
    }
    uVar3 = LZCOUNT(param_4) - LZCOUNT(param_2);
    param_4 = param_4 << (uVar3 & 0xff);
LAB_0801406a:
    param_4 = param_4 | param_3 >> (0x20 - uVar3 & 0xff);
    param_3 = param_3 << (uVar3 & 0xff);
    uVar3 = 0x80000000 >> (uVar3 & 0xff);
    while( true ) {
      bVar1 = param_2 - param_4 < (uint)(param_3 <= param_1);
      bVar6 = param_4 < param_2 || bVar1;
      if (param_4 < param_2 || bVar1) {
        param_2 = (param_2 - param_4) - (uint)(param_3 > param_1);
        param_1 = param_1 - param_3;
      }
      uVar4 = uVar3 * 2;
      bVar1 = CARRY4(uVar3,uVar3);
      uVar3 = uVar3 * 2 + (uint)bVar6;
      if (bVar1 || CARRY4(uVar4,(uint)bVar6)) break;
      uVar4 = param_4 & 1;
      param_4 = param_4 >> 1;
      param_3 = (uint)(uVar4 != 0) << 0x1f | param_3 >> 1;
    }
    return (ulonglong)uVar3;
  }
  if (param_2 == 0) {
    if (param_3 != 0) {
      return (ulonglong)(param_1 / param_3);
    }
  }
  else {
    if (1 < param_3) {
      if (param_3 >> 0x10 == 0) {
        uVar4 = (param_2 - param_3 * (param_2 / param_3)) * 0x10000 | param_1 >> 0x10;
        uVar3 = uVar4 / param_3;
        return CONCAT44(param_2 / param_3,
                        (param_1 & 0xffff | (uVar4 - param_3 * uVar3) * 0x10000) / param_3 |
                        uVar3 << 0x10);
      }
      uVar3 = (0x20 - LZCOUNT(param_2)) + LZCOUNT(param_3);
      param_4 = 0;
      if (0x1f < uVar3) {
        param_3 = param_3 << (uVar3 - 0x20 & 0xff);
        uVar4 = 0x80000000 >> (uVar3 - 0x20 & 0xff);
        uVar5 = 0;
        uVar3 = 0;
        while( true ) {
          bVar1 = param_2 - param_3 < (uint)(uVar3 <= param_1);
          bVar6 = param_3 < param_2 || bVar1;
          if (param_3 < param_2 || bVar1) {
            param_2 = (param_2 - param_3) - (uint)(uVar3 > param_1);
            param_1 = param_1 - uVar3;
          }
          bVar7 = CARRY4(uVar4,uVar4) || CARRY4(uVar4 * 2,(uint)bVar6);
          uVar4 = uVar4 * 2 + (uint)bVar6;
          uVar2 = uVar5 * 2;
          bVar1 = CARRY4(uVar5,uVar5);
          uVar5 = uVar5 * 2 + (uint)bVar7;
          if (bVar1 || CARRY4(uVar2,(uint)bVar7)) break;
          uVar2 = param_3 & 1;
          param_3 = param_3 >> 1;
          uVar3 = (uint)(uVar2 != 0) << 0x1f | uVar3 >> 1;
        }
        return CONCAT44(uVar5,uVar4);
      }
      goto LAB_0801406a;
    }
    if (param_3 != 0) {
      return CONCAT44(param_2,param_1);
    }
  }
  return CONCAT44(param_2,param_1);
}


// ==== __aeabi_l2f @ 08014118

/* library: int64 r0:r1 -> float (negate, ul2f core, sign); DC/AC VA averages 0x20007e20. library */

uint __aeabi_l2f(uint param_1,uint param_2)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  code *UNRECOVERED_JUMPTABLE;
  
  if ((int)param_2 < 0) {
    uVar1 = __iar_ul2f_core(-param_1,-(uint)(param_1 != 0) - param_2);
                    /* WARNING: Could not recover jumptable at 0x08014130. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar1 = (*UNRECOVERED_JUMPTABLE)(uVar1 | 0x80000000);
    return uVar1;
  }
  if (param_1 != 0 || param_2 != 0) {
    if (param_2 == 0) {
      uVar1 = param_1 << LZCOUNT(param_1);
      iVar3 = 0x9d - LZCOUNT(param_1);
    }
    else {
      uVar2 = param_1 << LZCOUNT(param_2);
      iVar3 = -LZCOUNT(param_2);
      uVar1 = iVar3 + 0x20U & 0x1f;
      uVar1 = ((param_2 ^ param_1) >> uVar1 | (param_2 ^ param_1) << 0x20 - uVar1) ^ uVar2;
      if (uVar2 != 0) {
        uVar1 = uVar1 | 1;
      }
      iVar3 = iVar3 + 0xbd;
    }
    param_1 = iVar3 * 0x800000 +
              (uVar1 >> 8) +
              (uint)(0x80000000 < uVar1 * 0x1000000 ||
                    uVar1 * 0x1000000 + 0x80000000 < (uint)((uVar1 & 0x100) != 0));
  }
  return param_1;
}


// ==== __iar_ul2f_core @ 08014140

/* library: interior of __aeabi_ul2f (entry 08014134) past its zero test; called by __aeabi_l2f.
   library */

int __iar_ul2f_core(uint param_1,uint param_2)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  char in_ZR;
  
  if (in_ZR == '\0') {
    uVar2 = param_1 << LZCOUNT(param_2);
    iVar3 = -LZCOUNT(param_2);
    uVar1 = iVar3 + 0x20U & 0x1f;
    uVar1 = ((param_2 ^ param_1) >> uVar1 | (param_2 ^ param_1) << 0x20 - uVar1) ^ uVar2;
    if (uVar2 != 0) {
      uVar1 = uVar1 | 1;
    }
    iVar3 = iVar3 + 0xbd;
  }
  else {
    uVar1 = param_1 << LZCOUNT(param_1);
    iVar3 = 0x9d - LZCOUNT(param_1);
  }
  return iVar3 * 0x800000 +
         (uVar1 >> 8) +
         (uint)(0x80000000 < uVar1 * 0x1000000 ||
               uVar1 * 0x1000000 + 0x80000000 < (uint)((uVar1 & 0x100) != 0));
}


// ==== Jack_Poll @ 0801417c  [GPIOE]

/* A/mA jack sense from main: PE10 low = 1 (mA jack), PE11 low = 2 (A jack); 10-tick debounce
   (0x2000023b), 0x20008075 = lead in mA jack; swaps A<->mA modes 20<->18, 21<->19, VA 15<->14,
   24<->23 (redraw via 0x0800d2d0(1)); 0x20008076 = wrong-jack " LEAD" warning when a lead is in a
   current jack in a non-current mode (uA modes: A jack only). high [video: jack sense, confirmed]
   (emulated: V + mA lead -> " LEAD"; A dial + mA lead -> mode 21 -> 19) */

void Jack_Poll(void)

{
  bool bVar1;
  bool bVar2;
  ushort uVar3;
  char cVar4;
  
  bVar2 = false;
  bVar1 = false;
  uVar3 = GPIOE.IDR._0_2_;
  cVar4 = -1 < (int)((uint)uVar3 << 0x15);
  uVar3 = GPIOE.IDR._0_2_;
  if (-1 < (int)((uint)uVar3 << 0x14)) {
    cVar4 = cVar4 + '\x02';
  }
  if (DAT_200080ea == cVar4) {
    DAT_2000023b = DAT_2000023b + -1;
    if (DAT_2000023b == '\0') {
      DAT_2000023b = '\n';
      DAT_20008075 = cVar4 == '\x01';
      bVar2 = true;
    }
  }
  else {
    DAT_2000023b = '\n';
    DAT_200080cc = cVar4;
    DAT_200080ea = cVar4;
  }
  if ((g_mode == '\x14') && (DAT_20008075 != '\0')) {
    g_mode = '\x12';
    bVar1 = true;
  }
  else if ((g_mode == '\x15') && (DAT_20008075 != '\0')) {
    g_mode = '\x13';
    bVar1 = true;
  }
  else if ((g_mode == '\x0f') && (DAT_20008075 != '\0')) {
    g_mode = '\x0e';
    bVar1 = true;
  }
  else if ((g_mode == '\x18') && (DAT_20008075 != '\0')) {
    g_mode = '\x17';
    bVar1 = true;
  }
  else if ((g_mode == '\x12') && (DAT_20008075 == '\0')) {
    g_mode = '\x14';
    bVar1 = true;
  }
  else if ((g_mode == '\x13') && (DAT_20008075 == '\0')) {
    g_mode = '\x15';
    bVar1 = true;
  }
  else if ((g_mode == '\x0e') && (DAT_20008075 == '\0')) {
    g_mode = '\x0f';
    bVar1 = true;
  }
  else if ((g_mode == '\x17') && (DAT_20008075 == '\0')) {
    g_mode = '\x18';
    bVar1 = true;
  }
  if (bVar1) {
    DAT_2000804a = 1;
    Mode_ApplyChange(1);
  }
  if (bVar2) {
    if ((cVar4 == '\x01') || (cVar4 == '\x02')) {
      if ((g_mode == '\x12') || (((g_mode == '\x14' || (g_mode == '\x13')) || (g_mode == '\x15'))))
      {
        DAT_20008076 = 0;
      }
      else if (((g_mode == '\x0e') || (g_mode == '\x0f')) ||
              ((g_mode == '\x17' || (g_mode == '\x18')))) {
        DAT_20008076 = 0;
      }
      else if (((g_mode == '\x10') || (g_mode == '\x11')) ||
              ((g_mode == '\r' || (g_mode == '\x16')))) {
        if (cVar4 == '\x02') {
          Jack_SetLeadWarning();
        }
        else {
          DAT_20008076 = 0;
        }
      }
      else {
        Jack_SetLeadWarning();
      }
    }
    else {
      DAT_20008076 = 0;
    }
  }
  return;
}


// ==== Jack_SetLeadWarning @ 080143e4

/* Jack_Poll: 0x20008076 = 1 (lead in the wrong jack: main shows " LEAD"). high */

void Jack_SetLeadWarning(void)

{
  DAT_20008076 = 1;
  return;
}


// ==== Mode_FromRotary @ 080143ee

/* WARNING: Removing unreachable block (ram,0x080144ec) */
/* on a dial change (0x20008064 via 0x08014954): one-hot g_rotary 0x20008065 -> g_mode (LowZ = 0,
   other positions from the per-position last-mode loaders 0x08010ff0.. reading EEPROM 0x08..0x0E;
   0x82 at 0x08 = ACV with AC+DC); clears HOLD/MIN-MAX/REL/LPF/peak state, sets redraw 0x2000804a.
   high */

void Mode_FromRotary(void)

{
  bool bVar1;
  
  bVar1 = false;
  if (DAT_20008065 == '\x01') {
    g_mode = 0;
    bVar1 = true;
  }
  else if (DAT_20008065 == '\x02') {
    g_mode = Mode_LastDcvAcv();
    bVar1 = true;
  }
  else if (DAT_20008065 == '\x04') {
    g_mode = Mode_LastMv();
    bVar1 = true;
  }
  else if (DAT_20008065 == '\b') {
    g_mode = Mode_LastHz();
    bVar1 = true;
  }
  else if (DAT_20008065 == '\x10') {
    g_mode = Mode_LastOhm();
    bVar1 = true;
  }
  else if (DAT_20008065 == ' ') {
    g_mode = Mode_LastVa();
    bVar1 = true;
  }
  else if (DAT_20008065 == '@') {
    g_mode = Mode_LastMicroAmp();
    bVar1 = true;
  }
  else if (DAT_20008065 == -0x80) {
    g_mode = Mode_LastMilliAmp();
    bVar1 = true;
  }
  if (bVar1) {
    DAT_2000801b = 0;
    DAT_2000801d = 0;
    DAT_2000804a = 1;
    g_lpf_1khz = 0;
    DAT_2000804f = 0;
    DAT_20008050 = 0;
    DAT_20008053 = 0;
    DAT_20008051 = 0;
    DAT_20007fb2 = 0;
    DAT_20008052 = 0;
  }
  return;
}


// ==== Mode_Cycle @ 08014500

/* MODE short press (from Key_ShortPress): next g_mode within the dial group: DCV->ACV->ACV+AC+DC
   (0x08014708 toggles 0x200080ba)->DCV; DCmV->ACmV->Temp; Hz->pulse->duty; Ohm->cont->diode->cap;
   uA: 13->17->16->22; mA 18<->19; A 20<->21; VA 14->23->14, 15->24->15; 0x20008074 = counter mode
   (6..8); saves the mode per position (0x08011154 -> EEPROM 0x08..0x0E); clears sub icons
   (0x0801a760). high [video: MODE cycle, confirmed] */

void Mode_Cycle(void)

{
  bool bVar1;
  
  bVar1 = false;
  LCD_ClearSubUnits();
  if (g_mode == 1) {
    g_mode = 2;
    bVar1 = true;
  }
  else if (g_mode != 0) {
    if (g_mode == 3) {
      g_mode = 4;
      bVar1 = true;
    }
    else if (g_mode < 3) {
      Mode_ToggleAcDc();
      bVar1 = true;
    }
    else if (g_mode == 5) {
      g_mode = 3;
      bVar1 = true;
    }
    else if (g_mode < 5) {
      g_mode = 5;
      bVar1 = true;
    }
    else if (g_mode == 7) {
      g_mode = 8;
      bVar1 = true;
    }
    else if (g_mode < 7) {
      g_mode = 7;
      bVar1 = true;
    }
    else if (g_mode == 9) {
      g_mode = 10;
      bVar1 = true;
    }
    else if (g_mode < 9) {
      g_mode = 6;
      bVar1 = true;
    }
    else if (g_mode == 0xb) {
      g_mode = 0xc;
      bVar1 = true;
    }
    else if (g_mode < 0xb) {
      g_mode = 0xb;
      bVar1 = true;
    }
    else if (g_mode == 0xd) {
      g_mode = 0x11;
      bVar1 = true;
    }
    else if (g_mode < 0xd) {
      g_mode = 9;
      bVar1 = true;
    }
    else if (g_mode == 0xf) {
      g_mode = 0x18;
      bVar1 = true;
    }
    else if (g_mode < 0xf) {
      g_mode = 0x17;
      bVar1 = true;
    }
    else if (g_mode == 0x11) {
      g_mode = 0x10;
      bVar1 = true;
    }
    else if (g_mode < 0x11) {
      g_mode = 0x16;
      bVar1 = true;
    }
    else if (g_mode == 0x13) {
      g_mode = 0x12;
      bVar1 = true;
    }
    else if (g_mode < 0x13) {
      g_mode = 0x13;
      bVar1 = true;
    }
    else if (g_mode == 0x15) {
      g_mode = 0x14;
      bVar1 = true;
    }
    else if (g_mode < 0x15) {
      g_mode = 0x15;
      bVar1 = true;
    }
    else if (g_mode == 0x17) {
      g_mode = 0xe;
      bVar1 = true;
    }
    else if (g_mode < 0x17) {
      g_mode = 0xd;
      bVar1 = true;
    }
    else if (g_mode == 0x18) {
      g_mode = 0xf;
      bVar1 = true;
    }
  }
  DAT_20008074 = 0;
  if (((g_mode == 6) || (g_mode == 7)) || (g_mode == 8)) {
    DAT_20008074 = 1;
  }
  if (bVar1) {
    DAT_2000804f = 0;
    DAT_20008050 = 0;
    DAT_2000804a = 1;
    Mode_SaveForPosition();
  }
  return;
}


// ==== Mode_ToggleAcDc @ 08014708

/* Mode_Cycle from ACV: AC+DC off -> Mode_AcDcOn, else AC+DC off and g_mode = 1 (DCV). high */

undefined4 Mode_ToggleAcDc(void)

{
  undefined4 unaff_r7;
  
  if (DAT_200080ba == '\0') {
    Mode_AcDcOn();
  }
  else {
    DAT_200080ba = '\0';
    g_mode = 1;
  }
  return unaff_r7;
}


// ==== Mode_AcDcOn @ 0801472c

/* WARNING: Removing unreachable block (ram,0x0801475e) */
/* AC+DC on (0x200080ba = 1) for ACV: 1 kHz LPF off and clears 0x20007e60, 0x200080a0,
   0x200080bb..be, 0x20007e9c/ea0 (related submode state); from the AC+DC toggle 08014708 and the
   dial-position loader 08010ff0 (EEPROM 0x08 = 0x82). medium */

void Mode_AcDcOn(void)

{
  DAT_200080ba = 1;
  g_lpf_1khz = 0;
  DAT_20007e60 = 0;
  DAT_200080a0 = 0;
  DAT_200080bd = 0;
  DAT_200080bc = 0;
  DAT_200080bb = 0;
  DAT_200080be = 0;
  DAT_20007e9c = 0;
  DAT_20007ea0 = 0;
  return;
}


// ==== Key_RangeShort @ 08014796

/* RANGE short press: HOLD/auto-hold on -> double beep; mode with < 2 ranges (0x20000000[mode]) ->
   double beep; first press in auto range only sets manual (0x20008042) and draws the icon (range
   unchanged), later presses (or any press in cal mode) step g_range cyclically, 0x2000804b = 1,
   VA_SetRangeFlags; short beep. high */

undefined4 Key_RangeShort(void)

{
  undefined4 unaff_r7;
  
  if (DAT_2000804f == '\0' && DAT_20008050 == '\0') {
    if (g_cal_mode == '\0' && DAT_20008042 == '\0') {
      if ((byte)(&DAT_20000000)[g_mode] < 2) {
        Beep_Double();
      }
      else {
        if (DAT_20008042 == '\0') {
          DAT_20008042 = '\x01';
          LCD_DrawAutoIcon(1);
        }
        Beep_Short();
      }
    }
    else if ((byte)(&DAT_20000000)[g_mode] < 2) {
      Beep_Double();
    }
    else {
      g_range = g_range + 1;
      if ((byte)(&DAT_20000000)[g_mode] <= g_range) {
        g_range = 0;
      }
      DAT_2000804b = 1;
      VA_SetRangeFlags(g_mode,g_range);
      Beep_Short();
    }
  }
  else {
    Beep_Double();
  }
  return unaff_r7;
}


// ==== VA_SetRangeFlags @ 0801485a

/* (mode, range) VA modes 13/14/15/22/23/24 only: range bit 0 -> V high 0x200080c6, bit 1 -> I high
   0x200080c7 (value 2 instead of 1 for the A-range VA modes 15/24); from Key_RangeShort and
   Key_LongPress (range 0). high */

void VA_SetRangeFlags(char param_1,byte param_2)

{
  if (((((param_1 == '\r') || (param_1 == '\x0e')) || (param_1 == '\x0f')) ||
      ((param_1 == '\x16' || (param_1 == '\x17')))) || (param_1 == '\x18')) {
    if (param_2 == 0) {
      DAT_200080c6 = 0;
      DAT_200080c7 = '\0';
    }
    else if (param_2 == 2) {
      DAT_200080c6 = 0;
      DAT_200080c7 = '\x01';
    }
    else if (param_2 < 2) {
      DAT_200080c6 = 1;
      DAT_200080c7 = '\0';
    }
    else if (param_2 == 3) {
      DAT_200080c6 = 1;
      DAT_200080c7 = '\x01';
    }
    if (((param_1 == '\x0f') || (param_1 == '\x18')) && (DAT_200080c7 == '\x01')) {
      DAT_200080c7 = '\x02';
    }
  }
  return;
}


// ==== VA_RangeFromFlags @ 080148f6

/* inverse of VA_SetRangeFlags: g_range = (V high 0x200080c6 ? 1 : 0) + (I high 0x200080c7 ? 2 : 0);
   from VA_VoltsAutorange / VA_AmpsAutorange. Never executed. high */

undefined1 VA_RangeFromFlags(void)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if ((DAT_200080c6 == '\0') || (DAT_200080c7 != '\0')) {
    if ((DAT_200080c6 == '\0') && (DAT_200080c7 != '\0')) {
      uVar1 = 2;
    }
    else if ((DAT_200080c6 != '\0') && (DAT_200080c7 != '\0')) {
      uVar1 = 3;
    }
  }
  else {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== Rotary_Poll @ 08014948

/* main 10 ms: Rotary_Debounce then Rotary_ApplyPending. high */

undefined4 Rotary_Poll(void)

{
  undefined4 unaff_r7;
  
  Rotary_Debounce();
  Rotary_ApplyPending();
  return unaff_r7;
}


// ==== Rotary_ApplyPending @ 08014954

/* dial change pending 0x20008064 -> clear, Mode_FromRotary. high */

undefined4 Rotary_ApplyPending(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008064 != '\0') {
    DAT_20008064 = '\0';
    Mode_FromRotary();
  }
  return unaff_r7;
}


// ==== Rotary_Debounce @ 0801496e

/* 10 ms tick (via 08014948): Rotary_Read; a position must read the same 5 times in a row
   (0x2000023c, count 0x200080ec), be one-hot (08016b7c), differ from the current 0x20008065, and
   the 15-tick lockout 0x200080eb be over; then 0x20008065 = it and the dial-change flag 0x20008064
   = 1. high */

undefined4 Rotary_Debounce(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  if (DAT_200080eb != '\0') {
    DAT_200080eb = DAT_200080eb + -1;
  }
  Rotary_Read();
  if (g_rotary == DAT_2000023c) {
    DAT_200080ec = DAT_200080ec + 1;
    if (4 < DAT_200080ec) {
      DAT_200080ec = 0;
      iVar1 = Bits_IsOneHot8(g_rotary);
      if (((iVar1 != 0) && (DAT_20008065 != g_rotary)) && (DAT_200080eb == '\0')) {
        DAT_20008065 = g_rotary;
        DAT_200080eb = '\x0f';
        DAT_20008064 = 1;
      }
    }
  }
  else {
    DAT_2000023c = g_rotary;
    DAT_200080ec = 0;
  }
  return unaff_r7;
}


// ==== Rotary_Read @ 08014a14  [GPIOG]

/* Rotary switch: GPIOG.IDR bits 7..0, active low, one-hot -> 0x20008066. high (Oct 31) */

void Rotary_Read(void)

{
  ushort uVar1;
  
  uVar1 = GPIOG.IDR._0_2_;
  g_rotary = -1 < (int)((uint)uVar1 << 0x18);
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x19)) {
    g_rotary = g_rotary | 2;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1a)) {
    g_rotary = g_rotary | 4;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1b)) {
    g_rotary = g_rotary | 8;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1c)) {
    g_rotary = g_rotary | 0x10;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1d)) {
    g_rotary = g_rotary | 0x20;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1e)) {
    g_rotary = g_rotary | 0x40;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x1f)) {
    g_rotary = g_rotary | 0x80;
  }
  return;
}


// ==== Key_FromBLE @ 08014ae6

/* BLE F4 key command -> key event (01-08 short, 81-88 long). high (Oct 31) */

undefined4 Key_FromBLE(void)

{
  undefined4 in_r3;
  char cVar1;
  
  cVar1 = '\0';
  DAT_20008069 = 0;
  DAT_2000801b = 0;
  DAT_2000801d = 0;
  if (DAT_2000806d == 1) {
    cVar1 = '\x10';
  }
  else if (DAT_2000806d != 0) {
    if (DAT_2000806d == 3) {
      cVar1 = '@';
    }
    else if (DAT_2000806d < 3) {
      cVar1 = ' ';
    }
    else if (DAT_2000806d == 5) {
      cVar1 = '\x01';
    }
    else if (DAT_2000806d < 5) {
      cVar1 = -0x80;
    }
    else if (DAT_2000806d == 7) {
      cVar1 = '\b';
    }
    else if (DAT_2000806d < 7) {
      cVar1 = '\x04';
    }
    else if (DAT_2000806d == 8) {
      cVar1 = '\x02';
    }
  }
  if (cVar1 == '\0') {
    if (DAT_2000806d == 0x81) {
      cVar1 = '\x10';
    }
    else {
      cVar1 = '\0';
      if (0x80 < DAT_2000806d) {
        if (DAT_2000806d == 0x83) {
          cVar1 = '@';
        }
        else if (DAT_2000806d < 0x83) {
          cVar1 = ' ';
        }
        else if (DAT_2000806d == 0x85) {
          cVar1 = '\x01';
        }
        else if (DAT_2000806d < 0x85) {
          cVar1 = -0x80;
        }
        else if (DAT_2000806d == 0x87) {
          cVar1 = '\b';
        }
        else if (DAT_2000806d < 0x87) {
          cVar1 = '\x04';
        }
        else if (DAT_2000806d == 0x88) {
          cVar1 = '\x02';
        }
      }
    }
    if (cVar1 != '\0') {
      Key_LongPress(cVar1);
    }
  }
  else {
    Key_ShortPress(cVar1);
  }
  return in_r3;
}


// ==== Key_ShortPress @ 08014c3c

/* short-press handler (switch on key bit); boot-screen actions too. [video] + calls here */

void Key_ShortPress(char param_1)

{
  int iVar1;
  
  if (param_1 == '\x01') {
    if (DAT_20007fe1 != '\0' || DAT_20007fe4 != '\0') {
      Beep_Double();
      return;
    }
    if (g_ms_countdown == 0) {
      Mode_Cycle();
    }
    else if ((g_boot_action == '\0') && (g_boot_sd_state == '\0')) {
      g_boot_action = '\x01';
      g_ms_countdown = 2000;
    }
    else if ((g_boot_action == '\0') && (g_boot_sd_state == '\x02')) {
      g_ms_countdown = 5000;
      g_boot_sd_state = 3;
      return;
    }
    Beep_Short();
    return;
  }
  if (param_1 == '\x02') {
    if (g_ms_countdown != 0) {
      if (g_boot_action != '\0') {
        return;
      }
      if (g_boot_sd_state == '\0') {
        g_ms_countdown = 500;
        g_boot_action = 3;
        return;
      }
      return;
    }
    if (DAT_20007fe1 != '\0' || DAT_20007fe4 != '\0') {
      Beep_Double();
      return;
    }
    if (g_cal_mode == '\0') {
      Setup_NextPage();
      Beep_Short();
      DAT_20008062 = 1;
      return;
    }
    iVar1 = Cal_CaptureRejected(0);
    if (iVar1 != 0) {
      Beep_Double();
      return;
    }
    iVar1 = Cal_ArmCapture(0);
    if (iVar1 == 2) {
      Beep_Double();
      return;
    }
  }
  else {
    if (param_1 == '\x04') {
      if (DAT_20007fe1 != '\0' || DAT_20007fe4 != '\0') {
        Beep_Double();
        return;
      }
      if (g_cal_mode != '\0') {
        Beep_Double();
        return;
      }
      if (g_mode == '\0') {
        Beep_Double();
        return;
      }
      if ((((DAT_2000804f == '\0') || (DAT_2000804e != '\0')) && (DAT_20008050 == '\0')) &&
         (DAT_200080b7 == 0)) {
        MinMax_KeyShort();
        Beep_Short();
        return;
      }
      Beep_Double();
      return;
    }
    if (param_1 != '\b') {
      if (param_1 == '\x10') {
        if (g_ms_countdown == 0) {
          if ((DAT_20007fe1 != '\0' || DAT_20007fe4 != '\0') || (DAT_2000804e != '\0')) {
            Beep_Double();
            return;
          }
          if (g_mode == '\v') {
            Setup_DiodeVoltage();
            Beep_Short();
            return;
          }
          Key_RangeShort();
          return;
        }
        if (g_boot_action != '\0') {
          return;
        }
        if (g_boot_sd_state == '\0') {
          g_ms_countdown = 2000;
          LCD_AllOn();
          LCD_Flush();
          return;
        }
        return;
      }
      if (param_1 == ' ') {
        if (DAT_20007fe1 != '\0' || DAT_20007fe4 != '\0') {
          Beep_Double();
          return;
        }
        if (DAT_2000804f == '\0' && DAT_20008050 == '\0') {
          DAT_2000804f = '\x01';
          DAT_20007d6c = DAT_20007d70;
          if (DAT_2000804e == '\0') {
            DAT_20008044 = DAT_20008042;
            DAT_20008042 = '\x01';
            LCD_DrawAutoIcon(0);
          }
          else {
            DAT_20007d6c = DAT_20007d9c;
          }
        }
        else if (DAT_2000804f == '\0') {
          DAT_2000804f = '\0';
          DAT_20008050 = '\0';
        }
        else {
          if (((g_mode == '\x06') || (g_mode == '\a')) ||
             ((g_mode == '\b' || ((g_mode == '\f' || (DAT_2000804e != '\0')))))) {
            DAT_2000804f = '\0';
            DAT_20008050 = '\0';
          }
          else {
            Hold_SetAutoHold(1,0);
          }
          if (DAT_2000804e == '\0') {
            DAT_20008042 = DAT_20008044;
            LCD_DrawAutoIcon(0);
          }
        }
        LCD_DrawAutoIcon(0);
        LCD_DrawHoldIcon(1);
        Beep_Short();
        return;
      }
      if (param_1 == '@') {
        if (g_ms_countdown == 0) {
          if (DAT_2000806e == '\0') {
            if (DAT_20007fe4 == '\0') {
              if (g_mode == '\0') {
                Beep_Double();
                return;
              }
              if ((DAT_20008048 & (DAT_20008058 ^ 1)) == 0) {
                DAT_20008058 = DAT_20008058 != 1;
                if ((bool)DAT_20008058) {
                  DAT_200080ed = DAT_20008042;
                  DAT_20008042 = '\x01';
                  if (DAT_2000804f == '\0') {
                    if (DAT_20008050 == '\0') {
                      DAT_20008059 = 1;
                    }
                    else {
                      DAT_20007da8 = DAT_20007dac;
                      LCD_DrawMainValue(g_mode,DAT_2000803f,0,0);
                    }
                  }
                  else {
                    DAT_20007da8 = DAT_20007d6c;
                    LCD_DrawMainValue(g_mode,DAT_2000803f,0,0);
                  }
                }
                else {
                  DAT_20008042 = DAT_200080ed != '\0';
                  DAT_20007da8 = 0;
                  if (DAT_2000804f == '\0') {
                    if (DAT_20008050 != '\0') {
                      DAT_20007da0 = DAT_20007dac;
                      LCD_DrawMainValue(g_mode,DAT_2000803f,DAT_20007dac,0);
                    }
                  }
                  else {
                    LCD_DrawMainValue(g_mode,DAT_2000803f,DAT_20007d6c,0);
                  }
                }
              }
              LCD_DrawAutoIcon(0);
              DAT_20008053 = 1;
              LCD_DrawRelIcon(1);
            }
            else {
              DAT_20007f9c = DAT_20007f9c + 1;
              if (DAT_20007f9e < DAT_20007f9c) {
                DAT_20007f9c = 1;
              }
              DAT_20007faa = 0;
              LogView_Show(DAT_20007f9c);
            }
          }
          else {
            Setup_Change(1);
          }
        }
        else if ((g_boot_action == '\x01') && (g_boot_sd_state == '\0')) {
          g_boot_action = '\x02';
          g_ms_countdown = 500;
        }
        else if ((g_boot_action == '\0') && (g_boot_sd_state == '\x01')) {
          g_boot_sd_state = '\x02';
          DAT_20008088 = 1;
          g_ms_countdown = 5000;
        }
        Beep_Short();
        return;
      }
      if (param_1 != -0x80) {
        return;
      }
      if (((g_ms_countdown != 0) && (g_boot_action == '\0')) && (g_boot_sd_state == '\x01')) {
        g_boot_sd_state = '\x02';
        DAT_20008089 = 1;
        g_ms_countdown = 5000;
      }
      if (DAT_2000806e != '\0') {
        Setup_Change(2);
        Beep_Short();
        return;
      }
      if (DAT_20007fe4 != '\0') {
        if (DAT_20007f9c == 1) {
          DAT_20007f9c = DAT_20007f9e;
        }
        else {
          DAT_20007f9c = DAT_20007f9c - 1;
        }
        LogView_Show(DAT_20007f9c);
        DAT_20007faa = 0;
        Beep_Short();
        return;
      }
      if (((g_mode != '\x02') || (DAT_200080ba != '\0')) && (g_mode != '\x04')) {
        Beep_Double();
        return;
      }
      DAT_200080b7 = DAT_200080b7 ^ 1;
      DAT_200080b8 = 0;
      DAT_2000804a = 1;
      DAT_20008042 = DAT_200080b7 != 0;
      if ((bool)DAT_20008042) {
        DAT_2000804e = '\0';
      }
      DAT_20007e8c = 0;
      DAT_20007e90 = 0;
      LCD_DrawPeakIcon(1);
      Beep_Short();
      return;
    }
  }
  if (g_ms_countdown == 0) {
    if (g_cal_mode == '\0') {
      if (DAT_20007fe1 == '\0') {
        if (DAT_20007fa4 == 0) {
          Beep_Double();
        }
        else {
          DAT_20007fe4 = DAT_20007fe4 != '\x01';
          if ((bool)DAT_20007fe4) {
            DAT_20007faa = 0;
            g_setup_page = 100;
            DAT_20007f9c = DAT_20007fa2;
            LCD_SetSubDP(0);
            LogView_LoadMode();
            LogView_DrawStatic();
            LogView_Show(DAT_20007f9c);
            Log_RestoreStartTime();
            Log_RestoreStopTime();
          }
          else {
            Display_ResetToLive();
          }
          LCD_DrawMemIcon(1);
          Beep_Short();
        }
      }
      else {
        Beep_Double();
      }
    }
    else if (DAT_2000808c == '\0' && DAT_2000808e == '\0') {
      iVar1 = Cal_CaptureRejected(1);
      if (iVar1 == 0) {
        iVar1 = Cal_ArmCapture(1);
        if (iVar1 == 2) {
          Beep_Double();
        }
        else {
          Beep_Short();
        }
      }
      else {
        Beep_Double();
      }
    }
    else {
      Beep_Double();
    }
  }
  else if ((g_boot_action == '\0') && (g_boot_sd_state == '\0')) {
    g_boot_sd_state = '\x01';
    g_ms_countdown = 5000;
    Beep_Short();
  }
  return;
}


// ==== Display_ResetToLive @ 080153ac

/* leave recall view (0x20007fe4 = 0), Setup_InitialPage, all dirty flags (bar, main, icons, units,
   sub); Mode_ApplyChange, Key_ShortPress, TIM2, main. high */

undefined4 Display_ResetToLive(void)

{
  undefined4 unaff_r7;
  
  DAT_20007fe4 = 0;
  Setup_InitialPage();
  DAT_2000805e = 1;
  g_display_update = 1;
  DAT_20008060 = 1;
  DAT_20008061 = 1;
  DAT_20008062 = 1;
  return unaff_r7;
}


// ==== Setup_NextPage @ 08015410

/* SETUP page counter 0x20008037; page count depends on mode. high */

undefined4 Setup_NextPage(void)

{
  undefined4 unaff_r7;
  
  if (g_setup_page != 8) {
    DAT_2000806e = '\0';
  }
  if ((DAT_2000806e == '\0') || (g_setup_page != 8)) {
    if ((g_mode == '\x02') && (DAT_200080ba == '\0')) {
      g_setup_page = g_setup_page + 1;
      if (0xb < g_setup_page) {
        g_setup_page = 0;
      }
    }
    else if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
             ((g_mode == '\x16' || (g_mode == '\x17')))) || (g_mode == '\x18')) {
      g_setup_page = g_setup_page + 1;
      if (g_setup_page == 0xb) {
        DAT_200080a9 = g_va_burden_display;
      }
      else if (g_va_burden_display == '\0') {
        if (0xb < g_setup_page) {
          g_setup_page = 0;
        }
      }
      else if (0xc < g_setup_page) {
        g_setup_page = 0;
      }
      if (g_setup_page == 10) {
        VA_SubSelectDecimals();
      }
      if (g_setup_page == 0xc) {
        DAT_200080aa = 100;
      }
    }
    else if (((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) {
      g_setup_page = g_setup_page + 1;
      if (g_setup_page == 10) {
        DAT_200080a6 = DAT_200080a5;
      }
      else if (0xb < g_setup_page) {
        g_setup_page = 0;
      }
    }
    else if (((g_mode == '\x11') || (g_mode == '\x13')) || (g_mode == '\x15')) {
      g_setup_page = g_setup_page + 1;
      if (g_setup_page == 10) {
        DAT_200080a6 = DAT_200080a5;
      }
      else if (DAT_200080a5 == '\0') {
        if (10 < g_setup_page) {
          g_setup_page = 0;
        }
      }
      else if (0xb < g_setup_page) {
        g_setup_page = 0;
      }
    }
    else if (((g_mode == '\v') || (g_mode == '\n')) || (g_mode == '\x04')) {
      g_setup_page = g_setup_page + 1;
      if (10 < g_setup_page) {
        g_setup_page = 0;
      }
    }
    else {
      g_setup_page = g_setup_page + 1;
      if (9 < g_setup_page) {
        g_setup_page = 0;
      }
    }
  }
  else {
    if (DAT_20008071 == '\0') {
      DAT_20008071 = '\x04';
    }
    else {
      DAT_20008071 = DAT_20008071 + -1;
    }
    DAT_20008070 = 0;
  }
  if (g_setup_page == 5) {
    DAT_200080dd = DAT_200080d7;
  }
  else if (g_setup_page == 6) {
    DAT_200080de = DAT_200080d8;
    DAT_200080df = DAT_200080d9;
    DAT_200080d6 = 0;
  }
  else if (g_setup_page == 7) {
    DAT_200080e0 = DAT_200080da;
    DAT_200080e1 = DAT_200080db;
    DAT_200080d6 = 0;
  }
  else if ((DAT_2000806e == '\0') && (g_setup_page == 8)) {
    DAT_20007d18 = g_meter_id;
  }
  else if (g_setup_page == 9) {
    DAT_20007fa6 = g_log_interval;
  }
  return unaff_r7;
}


// ==== Cal_CaptureRejected @ 08015728

/* 1 for modes/ranges without calibration (LowZ, pulse, duty; Hz/diode/cap on range > 1 for offset).
   high */

undefined1 Cal_CaptureRejected(char param_1)

{
  undefined1 uVar1;
  uint uVar2;
  
  uVar1 = 0;
  uVar2 = (uint)g_mode;
  if (uVar2 == 0) {
LAB_08015746:
    uVar1 = 1;
  }
  else {
    if (uVar2 != 6) {
      if (uVar2 - 7 < 2) goto LAB_08015746;
      if (1 < uVar2 - 0xb) {
        return 0;
      }
    }
    if ((param_1 == '\0') && (1 < g_range)) {
      uVar1 = 1;
    }
  }
  return uVar1;
}


// ==== Cal_ArmCapture @ 08015768

/* 0 = offset (0x2000808c), 1 = gain (0x2000808e), after Cal_AcFreqWindow. high (emulated) */

ulonglong Cal_ArmCapture(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  DAT_2000808c = 0;
  DAT_2000808d = 0;
  DAT_2000808e = 0;
  DAT_2000808f = 0;
  uVar1 = Cal_AcFreqWindow(param_1);
  if ((uVar1 & 0xff) == 0) {
    if (param_1 == '\0') {
      DAT_2000808c = 1;
    }
    else if (param_1 == '\x01') {
      DAT_2000808e = 1;
    }
    DAT_20008095 = Cal_CaptureBlocks();
    DAT_20007c38 = 0;
    DAT_20007c3c = 0;
    DAT_20007e1c = 0;
  }
  return CONCAT44(param_4,uVar1) & 0xffffffff000000ff;
}


// ==== Cal_CaptureBlocks @ 080157e8

/* Cal_ArmCapture: number of capture blocks (shown as nn in the -O-nn/-G-nn prompts, 0x20008095): 4
   for VA modes and Ohm range 6, else 16. high */

undefined1 Cal_CaptureBlocks(void)

{
  undefined1 uVar1;
  
  if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
      ((g_mode == '\x16' || (g_mode == '\x17')))) || (g_mode == '\x18')) {
    uVar1 = 4;
  }
  else if ((g_mode == '\t') && (g_range == '\x06')) {
    uVar1 = 4;
  }
  else {
    uVar1 = 0x10;
  }
  return uVar1;
}


// ==== Hold_SetAutoHold @ 0801584c

/* (on, from_log): HOLD 0x2000804f off, auto-hold 0x20008050 = on (saves g_range to 0x20008056,
   clears 0x20008055); resets 0x20008052, 0x20007fb2, sets 0x20008051/53 = on; from_log (Log_Toggle
   passes the logging flag) also turns MIN/MAX off and redraws the manual/hold icons. HOLD key
   second press (Key_ShortPress) uses (1, 0). Never executed. medium */

undefined4 Hold_SetAutoHold(char param_1,char param_2,undefined4 param_3,undefined4 param_4)

{
  DAT_2000804f = 0;
  DAT_20008050 = param_1 != '\0';
  if ((bool)DAT_20008050) {
    DAT_20008056 = g_range;
    DAT_20008055 = 0;
  }
  DAT_20008052 = 0;
  DAT_20007fb2 = 0;
  DAT_20008051 = DAT_20008050;
  DAT_20008053 = DAT_20008050;
  if (param_2 == '\x01') {
    LCD_DrawAutoIcon(0);
    DAT_2000804e = 0;
    DAT_20008058 = 0;
    LCD_DrawHoldIcon(1);
  }
  return param_4;
}


// ==== MinMax_KeyShort @ 080158d4

/* MIN/MAX short press: off -> init (08015974), manual flag saved to 0x20008044 and forced on (range
   locked), icon; on -> state 0x2000804e cycles 1..4 (MAX, MIN, AVG, live); then 08015934 (shows the
   selected value), icons, LCD_DrawMinMaxIcons(flush). high */

undefined4 MinMax_KeyShort(void)

{
  undefined4 unaff_r7;
  
  if (DAT_2000804e == 0) {
    MinMax_Init();
    DAT_20008044 = DAT_20008042;
    DAT_20008042 = 1;
    LCD_DrawAutoIcon(0);
  }
  else {
    DAT_2000804e = DAT_2000804e + 1;
    if (4 < DAT_2000804e) {
      DAT_2000804e = 1;
    }
  }
  MinMax_RedrawIfHold();
  LCD_DrawApoIcon(0);
  LCD_DrawMinMaxIcons(1);
  return unaff_r7;
}


// ==== MinMax_RedrawIfHold @ 08015934

/* when HOLD 0x2000804f is on: main display = MinMax_Selected(1) with decimals 0x2000803f, or " OFL"
   when 0x20008046; MinMax_KeyShort, Key_LongPress. medium */

void MinMax_RedrawIfHold(void)

{
  undefined4 uVar1;
  
  if (DAT_2000804f != '\0') {
    uVar1 = MinMax_Selected(1);
    if (DAT_20008046 == '\0') {
      LCD_DrawMainValue(g_mode,DAT_2000803f,uVar1,0);
    }
    else {
      LCD_ShowOFL(0);
    }
  }
  return;
}


// ==== MinMax_Init @ 08015974

/* MIN/MAX start: state 0x2000804e = 1 (MAX), max = INT_MIN, min = INT_MAX, mean/remainder/count 0.
   high */

void MinMax_Init(void)

{
  DAT_2000804e = 1;
  DAT_20007d88 = 0x80000000;
  DAT_20007d8c = 0x7fffffff;
  DAT_20007d90 = 0;
  DAT_20007d98 = 0;
  DAT_20007d94 = 0;
  return;
}


// ==== Setup_DiodeVoltage @ 080159c8

/* key in diode mode -> EEPROM 0x12. high */

undefined4 Setup_DiodeVoltage(void)

{
  undefined4 unaff_r7;
  
  g_diode_15v = g_diode_15v != '\x01';
  Settings_SaveDiodeV(g_diode_15v);
  DAT_2000803f = Display_DecimalPlaces(g_mode,g_range);
  Frontend_SetLedEnb(g_mode);
  Mode_SetFullScale();
  g_setup_page = 10;
  DAT_20008062 = 1;
  return unaff_r7;
}


// ==== Setup_Apo @ 08015a22

/* page 2 -> EEPROM 0x16. high */

void Setup_Apo(char param_1)

{
  if ((param_1 == '\x01') || (param_1 == '\x02')) {
    g_apo_off = g_apo_off != '\x01';
    LCD_DrawApoIcon(1);
  }
  else {
    Settings_SaveApo(g_apo_off);
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_Buzzer @ 08015a74

/* page 3 -> EEPROM 0x1E. high */

void Setup_Buzzer(char param_1)

{
  if ((param_1 == '\x01') || (param_1 == '\x02')) {
    if (g_buzzer_enabled == '\x01') {
      g_buzzer_enabled = '\0';
    }
    else {
      g_buzzer_enabled = '\x01';
    }
  }
  else {
    Settings_SaveBuzzer(g_buzzer_enabled);
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_LogInterval @ 08015ac0

/* page 9 -> EEPROM 0x18. high */

void Setup_LogInterval(char param_1)

{
  if (param_1 == '\x01') {
    if (DAT_20007fa6 < 999) {
      DAT_20007fa6 = DAT_20007fa6 + 1;
    }
    else {
      DAT_20007fa6 = 0;
    }
  }
  else if (param_1 == '\x02') {
    if (DAT_20007fa6 == 0) {
      DAT_20007fa6 = 999;
    }
    else {
      DAT_20007fa6 = DAT_20007fa6 - 1;
    }
  }
  else {
    g_log_interval = DAT_20007fa6;
    Settings_SaveLogInterval(DAT_20007fa6);
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_Contrast @ 08015b54

/* page 4 -> EEPROM 0x1A. high */

void Setup_Contrast(char param_1)

{
  if (param_1 == '\x01') {
    if (g_lcd_contrast == '\x11') {
      g_lcd_contrast = '\n';
    }
    else {
      g_lcd_contrast = g_lcd_contrast + '\x01';
    }
  }
  else if (param_1 == '\x02') {
    if (g_lcd_contrast == '\n') {
      g_lcd_contrast = '\x11';
    }
    else {
      g_lcd_contrast = g_lcd_contrast + -1;
    }
  }
  else {
    Settings_SaveContrast(g_lcd_contrast);
  }
  Board_LCDInit(g_lcd_contrast,0);
  DAT_20008062 = 1;
  return;
}


// ==== Setup_TempUnit @ 08015be0

/* page 0 -> EEPROM 0x10. high */

void Setup_TempUnit(char param_1)

{
  if ((param_1 == '\x01') || (param_1 == '\x02')) {
    g_temp_unit = g_temp_unit ^ 1;
    LCD_DrawTempUnitIcon();
  }
  else {
    Settings_SaveTempUnit(g_temp_unit);
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_Continuity @ 08015c20

/* page 10 in continuity -> EEPROM 0x14. high */

void Setup_Continuity(char param_1)

{
  if (param_1 == '\x01') {
    g_continuity_mode = g_continuity_mode + 1;
    if (3 < g_continuity_mode) {
      g_continuity_mode = 0;
    }
  }
  else if (param_1 == '\x02') {
    if (g_continuity_mode == 0) {
      g_continuity_mode = 3;
    }
    else {
      g_continuity_mode = g_continuity_mode - 1;
    }
  }
  else {
    Settings_SaveContinuity(g_continuity_mode);
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_Year @ 08015c94

/* page 5 (RTC; date stamp 0x01 in factory mode). high */

void Setup_Year(char param_1)

{
  if (param_1 == '\x01') {
    DAT_200080dd = DAT_200080dd + 1;
    if (99 < DAT_200080dd) {
      DAT_200080dd = 0;
    }
  }
  else if (param_1 == '\x02') {
    if (DAT_200080dd == 0) {
      DAT_200080dd = 99;
    }
    else {
      DAT_200080dd = DAT_200080dd - 1;
    }
  }
  else {
    DAT_200080d7 = DAT_200080dd;
    DAT_200080d9 = Date_ClampDay(DAT_200080dd,DAT_200080d8,DAT_200080d9);
    RTC_WriteClock();
    Settings_SaveDateStamp();
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_MonthDay @ 08015d40

/* page 6 (RTC; date stamp 0x01 in factory mode). high */

undefined4 Setup_MonthDay(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  
  if (DAT_200080d6 == '\0') {
    if (param_1 == '\x01') {
      DAT_200080de = DAT_200080de + 1;
      if (0xc < DAT_200080de) {
        DAT_200080de = 1;
      }
    }
    else if (param_1 == '\x02') {
      if (DAT_200080de < 2) {
        DAT_200080de = 0xc;
      }
      else {
        DAT_200080de = DAT_200080de - 1;
      }
    }
    else {
      DAT_200080d8 = DAT_200080de;
      DAT_200080d9 = Date_ClampDay(DAT_200080d7,DAT_200080de,DAT_200080d9);
      RTC_WriteClock();
      Settings_SaveDateStamp();
      DAT_200080d6 = '\x01';
      DAT_2000806e = 1;
    }
  }
  else {
    bVar1 = Date_DaysInMonth(DAT_200080d7,DAT_200080d8);
    if (param_1 == '\x01') {
      DAT_200080df = DAT_200080df + 1;
      if (bVar1 < DAT_200080df) {
        DAT_200080df = 1;
      }
    }
    else if (param_1 == '\x02') {
      if (DAT_200080df < 2) {
        DAT_200080df = bVar1;
      }
      else {
        DAT_200080df = DAT_200080df - 1;
      }
    }
    else {
      DAT_200080d9 = DAT_200080df;
      DAT_200080d9 = Date_ClampDay(DAT_200080d7,DAT_200080d8,DAT_200080df);
      RTC_WriteClock();
      DAT_200080d6 = '\0';
    }
  }
  DAT_20008062 = 1;
  return param_4;
}


// ==== Setup_HourMinute @ 08015e74

/* page 7 (RTC). high */

void Setup_HourMinute(char param_1)

{
  if (DAT_200080d6 == '\0') {
    if (param_1 == '\x01') {
      DAT_200080e0 = DAT_200080e0 + 1;
      if (0x17 < DAT_200080e0) {
        DAT_200080e0 = 0;
      }
    }
    else if (param_1 == '\x02') {
      if (DAT_200080e0 == 0) {
        DAT_200080e0 = 0x17;
      }
      else {
        DAT_200080e0 = DAT_200080e0 - 1;
      }
    }
    else {
      DAT_200080da = DAT_200080e0;
      RTC_WriteClock();
      DAT_200080d6 = '\x01';
      DAT_2000806e = 1;
    }
  }
  else if (param_1 == '\x01') {
    DAT_200080e1 = DAT_200080e1 + 1;
    if (0x3b < DAT_200080e1) {
      DAT_200080e1 = 0;
    }
  }
  else if (param_1 == '\x02') {
    if (DAT_200080e1 == 0) {
      DAT_200080e1 = 0x3b;
    }
    else {
      DAT_200080e1 = DAT_200080e1 - 1;
    }
  }
  else {
    DAT_200080db = DAT_200080e1;
    RTC_WriteClock();
    DAT_200080d6 = '\0';
  }
  DAT_20008062 = 1;
  return;
}


// ==== Settings_SaveDateStamp @ 08015f44

/* EEPROM 0x01 = YY*100+MM. high */

undefined4 Settings_SaveDateStamp(void)

{
  undefined4 unaff_r7;
  
  if (g_factory_mode != '\0') {
    g_date_stamp = (ushort)DAT_200080d7 * 100 + (ushort)DAT_200080d8;
    Settings_SaveDateStamp16();
  }
  return unaff_r7;
}


// ==== Setup_MeterId @ 08015f7c

/* page 8 -> EEPROM 0x03. high */

void Setup_MeterId(char param_1)

{
  uint local_20 [4];
  uint local_10;
  
  if (param_1 == 'd') {
    g_meter_id = DAT_20007d18;
    Settings_SaveMeterId();
  }
  else {
    local_20[0] = DAT_20007d18 % 10;
    local_20[1] = (DAT_20007d18 % 100) / 10;
    local_20[2] = (DAT_20007d18 % 1000) / 100;
    local_20[3] = (DAT_20007d18 % 10000) / 1000;
    local_10 = DAT_20007d18 / 10000;
    if (param_1 == '\x01') {
      local_20[DAT_20008071] = local_20[DAT_20008071] + 1;
      if (9 < local_20[DAT_20008071]) {
        local_20[DAT_20008071] = 0;
      }
    }
    else if (local_20[DAT_20008071] == 0) {
      local_20[DAT_20008071] = 9;
    }
    else {
      local_20[DAT_20008071] = local_20[DAT_20008071] - 1;
    }
    DAT_20007d18 = local_20[0] +
                   local_20[1] * 10 + local_20[2] * 100 + local_10 * 10000 + local_20[3] * 1000;
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_CurrentBurden @ 080160e8

/* WARNING: Removing unreachable block (ram,0x0801614e) */
/* WARNING: Removing unreachable block (ram,0x08016136) */
/* page 10 in AC/DC current (RAM only). high */

void Setup_CurrentBurden(char param_1)

{
  if (param_1 == 'd') {
    DAT_200080a5 = DAT_200080a6;
    g_tdm_channel = 0;
    DAT_200080c5 = 0;
    if (((g_mode == '\x11') || (g_mode == '\x13')) || (g_mode == '\x15')) {
      Mode_DCI_SelectChannel(0);
    }
    else {
      Mode_ACI_SelectChannel(0);
    }
  }
  else if (DAT_200080a6 == '\x01') {
    DAT_200080a6 = '\0';
  }
  else {
    DAT_200080a6 = '\x01';
  }
  DAT_20008062 = 1;
  return;
}


// ==== Setup_VaBurden @ 08016190

/* page 11 in VA modes -> EEPROM 0x1B. high */

void Setup_VaBurden(char param_1)

{
  if (param_1 == 'd') {
    g_va_burden_display = DAT_200080a9;
    Settings_SaveVaBurden(DAT_200080a9);
  }
  else if (DAT_200080a9 == '\x01') {
    DAT_200080a9 = '\0';
  }
  else {
    DAT_200080a9 = '\x01';
  }
  DAT_20008062 = 1;
  return;
}


// ==== Key_LongPress @ 0801622c  [GPIOE]

/* long-press handler (Oct 31 list). high */

void Key_LongPress(char param_1)

{
  int iVar1;
  
  if (param_1 == '\x01') {
    if (DAT_2000807b != '\0') {
      GPIOE.BSRR._2_2_ = 0x100;
    }
    else {
      DAT_20007fb8 = 0x78;
      GPIOE.BSRR._0_2_ = 0x100;
    }
    DAT_2000807b = DAT_2000807b == '\0';
    Beep_Long();
  }
  else if (param_1 == '\x02') {
    if (DAT_20007fe1 == '\0' && DAT_20007fe4 == '\0') {
      iVar1 = Setup_Change(0);
      if (iVar1 == 0) {
        Beep_Double();
      }
      else {
        DAT_2000806e = DAT_2000806e != '\x01';
        DAT_20008070 = 0;
        DAT_20008071 = 4;
        Beep_Long();
        if (DAT_2000806e == '\0') {
          Setup_Change(100);
        }
      }
    }
    else {
      Beep_Double();
    }
  }
  else if (param_1 == '\x04') {
    if (g_cal_mode == '\0') {
      if (DAT_2000804e == '\0') {
        Beep_Double();
      }
      else {
        DAT_2000804e = 4;
        MinMax_RedrawIfHold();
        if (DAT_2000804f == '\0') {
          DAT_20008042 = DAT_20008044;
          LCD_DrawAutoIcon(0);
        }
        DAT_2000804e = '\0';
        LCD_DrawMinMaxIcons(0);
        LCD_DrawApoIcon(0);
        Beep_Long();
      }
    }
    else {
      Beep_Double();
    }
  }
  else if (param_1 == '\b') {
    if (g_cal_mode == '\0') {
      Log_Toggle(0);
    }
    else {
      if ((g_mode == '\t') && (g_range == '\x06')) {
        DAT_20008090 = 1;
        DAT_20008095 = 0x10;
      }
      else {
        DAT_20008096 = 1;
      }
      Beep_Long();
    }
  }
  else if (param_1 == '\x10') {
    if (DAT_20007fe1 == '\0' && DAT_20007fe4 == '\0') {
      if (g_cal_mode == '\0') {
        DAT_20008042 = 0;
        g_range = '\0';
        VA_SetRangeFlags(g_mode,0);
        DAT_2000804b = 1;
      }
      LCD_DrawAutoIcon(1);
      Beep_Long();
    }
    else {
      Beep_Double();
    }
  }
  else if (param_1 == '(') {
    if (g_cal_mode == '\0') {
      Log_Toggle(1);
    }
    else {
      Beep_Double();
    }
  }
  else if (param_1 == '@') {
    if ((DAT_20007fe1 == '\0' && DAT_20007fe4 == '\0') && DAT_2000806e == '\0') {
      if ((g_mode == '\x02') || (g_mode == '\x04')) {
        if (DAT_200080ba == '\0') {
          g_lpf_1khz = g_lpf_1khz ^ 1;
          DAT_200080ba = '\0';
          DAT_2000804a = 1;
          Beep_Short();
        }
        else {
          Beep_Double();
        }
      }
      else if (((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
               (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))) &&
              (g_range == '\0')) {
        DAT_20007e0c = g_reading + DAT_20007e0c;
        CurrentZero_Store(g_mode,0,g_reading);
        Beep_Long();
      }
      else {
        Beep_Double();
      }
    }
    else {
      Beep_Double();
    }
  }
  else if (param_1 == -0x80) {
    if (DAT_2000806e == '\0') {
      g_ble_on = g_ble_on != '\x01';
      DAT_20008033 = 0;
      DAT_20008069 = 0;
      DAT_2000806d = 0;
      BLE_Toggle(g_ble_on);
      DAT_20007fb0 = 0;
      Beep_Long();
    }
    else {
      Beep_Double();
    }
  }
  else {
    Beep_Double();
  }
  return;
}


// ==== Log_Toggle @ 0801657c

/* long MEM (key 8; also key 0x28): logging flag 0x20007fe1 toggled, then Log_Start or Log_Stop.
   high (emulated) */

void Log_Toggle(char param_1)

{
  DAT_20007fe1 = DAT_20007fe1 != '\x01';
  if ((param_1 != '\0') || (DAT_20008050 != '\0')) {
    Hold_SetAutoHold(DAT_20007fe1,1);
  }
  if (DAT_20007fe1 == '\0') {
    Log_Stop(1);
  }
  else {
    Log_Start();
  }
  LCD_DrawApoIcon(0);
  LCD_DrawMemIcon(1);
  Beep_Long();
  return;
}


// ==== Setup_Change @ 080165f4

/* UP/DN on a SETUP page -> that page's handler. high */

undefined8 Setup_Change(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  
  bVar1 = 0;
  DAT_20008070 = 0;
  if (((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
      (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) && (g_setup_page == '\v'))
  {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_VaBurden(param_1);
    }
  }
  if ((((g_mode == '\x10') || (g_mode == '\x12')) ||
      ((g_mode == '\x14' || (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))))) &&
     (g_setup_page == '\n')) {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_CurrentBurden(param_1);
    }
  }
  else if ((g_mode == '\n') && (g_setup_page == '\n')) {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_Continuity(param_1);
    }
  }
  else if (g_setup_page == '\0') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_TempUnit(param_1);
    }
  }
  else if (g_setup_page == '\x02') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_Apo(param_1);
    }
  }
  else if (g_setup_page == '\x03') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_Buzzer(param_1);
    }
  }
  else if (g_setup_page == '\x04') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_Contrast(param_1);
    }
  }
  else if (g_setup_page == '\t') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_LogInterval(param_1);
    }
  }
  else if (g_setup_page == '\x05') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_Year(param_1);
    }
  }
  else if (g_setup_page == '\x06') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_MonthDay(param_1);
    }
  }
  else if (g_setup_page == '\a') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_HourMinute(param_1);
    }
  }
  else if (g_setup_page == '\b') {
    if (param_1 == '\0') {
      bVar1 = 1;
    }
    else {
      Setup_MeterId(param_1);
    }
  }
  return CONCAT44(param_4,(uint)bVar1);
}


// ==== Keys_Process @ 0801680c

/* every 10 ms tick: Keys_Scan, Keys_Dispatch, then a pending BLE key (Key_FromBLE). high */

undefined4 Keys_Process(void)

{
  undefined4 unaff_r7;
  
  Keys_Scan();
  Keys_Dispatch();
  if (DAT_20008069 != '\0') {
    Key_FromBLE(DAT_2000806d);
  }
  return unaff_r7;
}


// ==== Keys_Dispatch @ 0801682c

/* short -> Key_ShortPress(code), long -> Key_LongPress(code). high */

undefined4 Keys_Dispatch(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20008067 != '\0') {
    DAT_20008067 = '\0';
    DAT_2000801b = 0;
    DAT_2000801d = 0;
    Key_ShortPress(DAT_2000806b);
  }
  if (DAT_20008068 != '\0') {
    DAT_20008068 = '\0';
    DAT_2000801b = 0;
    DAT_2000801d = 0;
    Key_LongPress(DAT_2000806c);
  }
  return unaff_r7;
}


// ==== Keys_Scan @ 0801688c

/* debounce 2 samples; short press on release, long press at 50 ticks (500 ms); during the boot
   screen a press fires at once. high */

undefined4 Keys_Scan(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  if (DAT_20008073 != '\0') {
    DAT_20008073 = DAT_20008073 + -1;
  }
  Keys_Read();
  if (DAT_2000806a == DAT_200080f0) {
    DAT_200080ef = DAT_200080ef + 1;
    if (1 < DAT_200080ef) {
      if (DAT_2000806a == '\0') {
        DAT_20008072 = '\0';
        DAT_200080ef = 2;
        if (DAT_200080ee == '\x01') {
          DAT_200080ee = '\0';
          DAT_20008073 = '\n';
          DAT_20008067 = 1;
          DAT_2000806b = DAT_200080f1;
        }
      }
      else {
        iVar1 = Bits_IsOneHot8(DAT_2000806a);
        if ((iVar1 != 0) || (iVar1 = Key_IsCombo28(DAT_2000806a), iVar1 != 0)) {
          if (DAT_20008072 == '\0') {
            if (DAT_20008073 == '\0') {
              DAT_200080ef = 2;
              DAT_20008072 = '\x01';
              Keys_AutoRepeat(0);
              if ((g_ms_countdown == 0) || (g_boot_sd_state != '\0')) {
                if ((DAT_20007fe4 == '\0') || ((DAT_2000806a != '@' && (DAT_2000806a != -0x80)))) {
                  DAT_200080ee = '\x01';
                  DAT_200080f1 = DAT_2000806a;
                }
              }
              else {
                DAT_200080ee = '\0';
                DAT_20008073 = '\x05';
                DAT_20008067 = 1;
                DAT_2000806b = DAT_2000806a;
              }
            }
          }
          else {
            DAT_20008072 = DAT_20008072 + '\x01';
            if (DAT_20008072 == '2') {
              DAT_20008072 = '3';
              DAT_200080ee = '\0';
              iVar1 = Key_AllowsLong(DAT_2000806a);
              if (iVar1 != 0) {
                DAT_20008068 = 1;
                DAT_2000806c = DAT_2000806a;
                DAT_20008073 = '\n';
              }
            }
            else if ((DAT_20007fe4 != '\0' || DAT_2000806e != '\0') &&
                    ((DAT_2000806a == '@' || (DAT_2000806a == -0x80)))) {
              DAT_20008072 = '\x01';
              Keys_AutoRepeat(1);
            }
          }
        }
      }
    }
  }
  else {
    DAT_200080f0 = DAT_2000806a;
    DAT_200080ef = 0;
  }
  return unaff_r7;
}


// ==== Keys_AutoRepeat @ 08016a64

/* (held) from Keys_Scan for the up/down keys (0x40/0x80) in the recall view or SETUP edit: 0
   resets; first repeat after 50 ticks (3 in the recall view 0x20007fe4), then 5 repeats every 30
   ticks, then every 10 ticks; each repeat queues a short press (0x20008067, code 0x2000806b = held
   key). high */

void Keys_AutoRepeat(char param_1)

{
  byte bVar1;
  uint uVar2;
  
  if (param_1 == '\0') {
    DAT_200080f2 = 0;
    DAT_200080f3 = 0;
  }
  else if (DAT_200080f2 == 0) {
    if (DAT_20007fe4 == '\0') {
      bVar1 = 0x32;
    }
    else {
      bVar1 = 3;
    }
    DAT_200080f3 = DAT_200080f3 + 1;
    if (bVar1 < DAT_200080f3) {
      DAT_200080f2 = 1;
      DAT_200080f3 = 0;
      DAT_20008067 = 1;
      DAT_2000806b = DAT_2000806a;
    }
  }
  else {
    uVar2 = DAT_200080f2 - 1;
    if (uVar2 < 5) {
      DAT_200080f3 = DAT_200080f3 + 1;
      if (0x1e < DAT_200080f3) {
        DAT_200080f2 = DAT_200080f2 + 1;
        DAT_200080f3 = 0;
        DAT_20008067 = 1;
        DAT_2000806b = DAT_2000806a;
      }
    }
    else if ((uVar2 == 5) && (DAT_200080f3 = DAT_200080f3 + 1, 10 < DAT_200080f3)) {
      DAT_200080f2 = 6;
      DAT_200080f3 = 0;
      DAT_20008067 = 1;
      DAT_2000806b = DAT_2000806a;
    }
  }
  return;
}


// ==== Key_AllowsLong @ 08016b4c

/* (key code) 1 for single keys 0x01..0x80 except 0x20, plus the combination 0x28; Keys_Scan long
   press. high */

undefined1 Key_AllowsLong(char param_1)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if (((((param_1 == '\x01') || (param_1 == '\x02')) || (param_1 == '\x04')) ||
      ((param_1 == '\b' || (param_1 == '\x10')))) ||
     ((param_1 == '(' || ((param_1 == '@' || (param_1 == -0x80)))))) {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== Bits_IsOneHot8 @ 08016b7c

/* (byte) 1 when exactly one of bits 0..7 is set; Rotary_Debounce and Keys_Scan. high */

undefined1 Bits_IsOneHot8(char param_1)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if (((((param_1 == '\x01') || (param_1 == '\x02')) || (param_1 == '\x04')) ||
      ((param_1 == '\b' || (param_1 == '\x10')))) ||
     ((param_1 == ' ' || ((param_1 == '@' || (param_1 == -0x80)))))) {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== Key_IsCombo28 @ 08016bb8

/* (key code) == 0x28 (two keys together, 0x20 + 0x08); Keys_Scan. Never executed. high */

bool Key_IsCombo28(char param_1)

{
  return param_1 == '(';
}


// ==== Keys_Read @ 08016bca  [GPIOG]

/* PG8..15 keys (active low). high */

void Keys_Read(void)

{
  ushort uVar1;
  
  uVar1 = GPIOG.IDR._0_2_;
  DAT_2000806a = -1 < (int)((uint)uVar1 << 0x17);
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x16)) {
    DAT_2000806a = DAT_2000806a | 2;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x15)) {
    DAT_2000806a = DAT_2000806a | 4;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x14)) {
    DAT_2000806a = DAT_2000806a | 8;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x13)) {
    DAT_2000806a = DAT_2000806a | 0x10;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x12)) {
    DAT_2000806a = DAT_2000806a | 0x20;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x11)) {
    DAT_2000806a = DAT_2000806a | 0x40;
  }
  uVar1 = GPIOG.IDR._0_2_;
  if (-1 < (int)((uint)uVar1 << 0x10)) {
    DAT_2000806a = DAT_2000806a | 0x80;
  }
  return;
}


// ==== LCD_RefreshDirty @ 08016d34

/* main loop (not in the recall view): redraws the parts flagged dirty: 0x2000805e (08018b14), main
   (g_display_update/0x2000805f, LCD_DrawMain), icons + units (0x20008060), unit prefix
   (0x20008061), units (0x2000804c), sub units on a SETUP page change or 0x20008063, sub
   (0x20008062, LCD_DrawSub); LCD_Flush when 0x2000805b. high */

undefined4 LCD_RefreshDirty(void)

{
  undefined4 unaff_r7;
  
  if (DAT_20007fe4 == '\0') {
    if (DAT_2000805e != '\0') {
      Bar_RedrawDirty();
    }
    if (g_display_update != '\0' || DAT_2000805f != '\0') {
      LCD_DrawMain();
    }
    if (DAT_20008060 != '\0') {
      LCD_RedrawIconsUnits();
    }
    if (DAT_20008061 != '\0') {
      LCD_RedrawUnitPrefix();
    }
    if (DAT_2000804c != '\0') {
      LCD_DrawUnits(g_mode,g_range,DAT_20008098,DAT_20008099);
      DAT_2000805b = '\x01';
    }
    if ((DAT_20000236 != g_setup_page) || (DAT_20008063 != '\0')) {
      DAT_20000236 = g_setup_page;
      DAT_20008063 = '\0';
      LCD_DrawSubUnits();
    }
    if (DAT_20008062 != '\0') {
      LCD_DrawSub();
    }
    if (DAT_2000805b != '\0') {
      DAT_2000805b = '\0';
      LCD_Flush();
    }
  }
  return unaff_r7;
}


// ==== LCD_DrawMain @ 08016e08

/* main 5-digit display, in priority: Ohm hold-off countdown 0x2000805a; " LEAD" (0x20008076);
   "-----" in VA modes (0x200080bf); calibration prompts -O-nn/-G-nn/-4-nn/-5-nn/-3-nn (0x08019698,
   nn = 0x20008095) and their end texts OFF-E/G-END/400-E/5KEND (0x08019738) from flags
   0x2000808c..0x20008094; blank; " OFL" (0x20008046); else g_reading via 0x08017100 (|value| >=
   100000 shown /10 with one less decimal when 0x20008058). high */

undefined4 LCD_DrawMain(void)

{
  int iVar1;
  undefined4 in_r3;
  char cVar2;
  
  iVar1 = g_reading;
  g_display_update = 0;
  DAT_2000805f = 0;
  if ((((g_mode == '\t') && (DAT_20008042 == '\0' && g_cal_mode == '\0')) && (DAT_2000804e == '\0'))
     && (DAT_2000805a != '\0')) {
    DAT_2000805a = DAT_2000805a + -1;
  }
  else if (DAT_20008076 == '\0') {
    if (((((g_mode == '\r') || (g_mode == '\x0e')) ||
         ((g_mode == '\x0f' || ((g_mode == '\x16' || (g_mode == '\x17')))))) || (g_mode == '\x18'))
       && (DAT_200080bf != '\0')) {
      LCD_ShowDashes(0);
    }
    else if (DAT_2000808c == '\0') {
      if (DAT_2000808d == '\0') {
        if (DAT_2000808e == '\0') {
          if (DAT_20008090 == '\0') {
            if (DAT_2000808f == '\0') {
              if (DAT_20008091 == '\0') {
                if (DAT_20008092 == '\0') {
                  if (DAT_20008093 == '\0') {
                    if (DAT_20008094 == '\0') {
                      if (((DAT_20000238 == '\0') && (DAT_2000805c == '\0')) || (g_mode == '\0')) {
                        if (DAT_2000804f == '\0' && DAT_20008046 == '\x01') {
                          LCD_ShowOFL(0);
                          DAT_20008046 = '\0';
                        }
                        else if ((g_reading < 100000) && (-100000 < g_reading)) {
                          LCD_DrawMainValue(g_mode,DAT_2000803f,g_reading,1);
                        }
                        else if (DAT_20008058 != '\0') {
                          cVar2 = DAT_2000803f;
                          if (DAT_2000803f != '\0') {
                            cVar2 = DAT_2000803f + -1;
                          }
                          LCD_DrawMainValue(g_mode,cVar2,g_reading / 10,1);
                          DAT_20007d70 = iVar1;
                        }
                      }
                      else {
                        if (DAT_20008050 == '\0') {
                          LCD_ShowBlank(0);
                        }
                        DAT_20008046 = '\0';
                      }
                    }
                    else {
                      DAT_20008094 = '\0';
                      DAT_20008046 = '\0';
                      LCD_CalEndText(3);
                    }
                  }
                  else {
                    DAT_20008046 = '\0';
                    LCD_CalPrompt(3);
                  }
                }
                else {
                  DAT_20008092 = '\0';
                  DAT_20008046 = '\0';
                  LCD_CalEndText(2);
                }
              }
              else {
                DAT_20008046 = '\0';
                LCD_CalPrompt(2);
              }
            }
            else {
              DAT_2000808f = '\0';
              DAT_20008046 = '\0';
              LCD_CalEndText(1);
            }
          }
          else {
            DAT_20008046 = '\0';
            LCD_CalPrompt(4);
          }
        }
        else {
          DAT_20008046 = '\0';
          LCD_CalPrompt(1);
        }
      }
      else {
        DAT_2000808d = '\0';
        DAT_20008046 = '\0';
        LCD_CalEndText(0);
      }
    }
    else {
      DAT_20008046 = '\0';
      LCD_CalPrompt(0);
    }
  }
  else {
    LCD_ShowLead(0);
    DAT_20008046 = '\0';
  }
  return in_r3;
}


// ==== LCD_ShowOff @ 08017092

/* clear shadow and show "-oFF-" on the main display (flushes); from the power-off screen 08024f06
   (1 s beep). high */

undefined4 LCD_ShowOff(void)

{
  undefined4 unaff_r7;
  
  LCD_ClearShadow();
  LCD_Flush();
  LCD_DrawText5(0,"-oFF-");
  LCD_Flush();
  return unaff_r7;
}


// ==== LogView_Show @ 080170ac

/* (view position) recall view: position number on the sub (LCD_DrawSubValue),
   Record_Load(LogView_PosToSlot - 1), main value 0x20007d1c with the record's mode/decimals, or the
   OL text (080199b4) for 0x7FFFFFFF; LCD_Flush. From Key_ShortPress (enter, next, previous). Never
   executed. high */

void LogView_Show(undefined2 param_1)

{
  short sVar1;
  
  LCD_DrawSubValue(param_1,0,0);
  sVar1 = LogView_PosToSlot(param_1);
  Log_LoadRecord(sVar1 + -1);
  if (DAT_20007d1c == 0x7fffffff) {
    LCD_ShowOFL(0);
  }
  else {
    LCD_DrawMainValue(DAT_20007feb,DAT_20007fed,DAT_20007d1c,1);
  }
  LCD_Flush();
  return;
}


// ==== LCD_DrawMainValue @ 08017100

/* (mode, decimals, value, honor_hold): unless honor_hold and HOLD 0x2000804f is on: decimal point
   (LCD_SetMainDP), minus sign (0x2000779c bit 7), |value| to text (0801b090, Num_ToText) drawn on
   the main digits (08019ae0(0, 0x20007c30)), dirty flag 0x2000805b; last shown value kept in
   0x20007d70 (HOLD captures it). high */

void LCD_DrawMainValue(undefined4 param_1,undefined1 param_2,int param_3,char param_4)

{
  int iVar1;
  
  if ((param_4 == '\0') || (DAT_2000804f == '\0')) {
    LCD_SetMainDP(param_2);
    if (param_3 < 0) {
      DAT_2000779c = DAT_2000779c | 0x80;
      iVar1 = -param_3;
    }
    else {
      DAT_2000779c = DAT_2000779c & 0xffffff7f;
      iVar1 = param_3;
    }
    Num_SplitDigits(iVar1);
    Num_ToText(param_2);
    LCD_DrawText5(0,&DAT_20007c30);
    DAT_2000805b = 1;
    DAT_20007d70 = param_3;
  }
  return;
}


// ==== LCD_SetMainDP @ 0801717e

/* (decimals 0..4) main display decimal point: clears all, then 1 -> 0x20007790 bit 14, 2 ->
   0x20007794 bit 1, 3 -> bit 3, 4 -> bit 5 (V-x.xx boot text uses 2; SAvEd/Err 0). high */

void LCD_SetMainDP(byte param_1)

{
  DAT_20007790 = DAT_20007790 & 0xffffbfff;
  DAT_20007794 = DAT_20007794 & 0xfd5;
  if (param_1 == 1) {
    DAT_20007790 = DAT_20007790 | 0x4000;
  }
  else if (param_1 != 0) {
    if (param_1 == 3) {
      DAT_20007794 = DAT_20007794 | 8;
    }
    else if (param_1 < 3) {
      DAT_20007794 = DAT_20007794 | 2;
    }
    else if (param_1 == 4) {
      DAT_20007794 = DAT_20007794 | 0x20;
    }
  }
  return;
}


// ==== LCD_UpdateMainDP @ 080171fa

/* 0x2000803f = Display_DecimalPlaces(g_mode, g_range), LCD_SetMainDP, flush; Mode_ApplyChange,
   VA_ApplyRange. high */

undefined4 LCD_UpdateMainDP(void)

{
  undefined4 unaff_r7;
  
  DAT_2000803f = Display_DecimalPlaces(g_mode,g_range);
  LCD_SetMainDP(DAT_2000803f);
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_RedrawIconsUnits @ 08017222

/* LCD_RefreshDirty flag 0x20008060: LCD_DrawIcons, LCD_ClearUnits, LCD_DrawUnits, Bar_Reset, flush
   pending. high */

undefined4 LCD_RedrawIconsUnits(void)

{
  undefined4 unaff_r7;
  
  DAT_20008060 = 0;
  LCD_DrawIcons();
  LCD_ClearUnits();
  LCD_DrawUnits(g_mode,g_range,DAT_20008098,DAT_20008099);
  Bar_Reset();
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_DrawIcons @ 0801725e

/* redraws every annunciator group (13 icon routines: manual, hold, MIN/MAX, AC/DC, ...) without
   flushing; from 08017222 (mode change) and LogView_DrawStatic. medium */

undefined4 LCD_DrawIcons(void)

{
  undefined4 unaff_r7;
  
  LCD_DrawApoIcon(0);
  LCD_DrawLowBatIcon(0);
  LCD_DrawBtIcon(0);
  LCD_DrawAutoIcon(0);
  LCD_DrawHoldIcon(0);
  LCD_DrawMinMaxIcons(0);
  LCD_DrawRelIcon(0);
  LCD_DrawPeakIcon(0);
  LCD_DrawLpfIcon();
  LCD_DrawAcDc();
  LCD_DrawHvIcon();
  LCD_DrawTempUnitIcon();
  LCD_DrawMemIcon(0);
  return unaff_r7;
}


// ==== LCD_DrawUnits @ 080172a8

/* (mode, range, ac, show_acdc): main unit icons per g_mode: V, mV, ms (pulse), % (duty), OHM+BEEP
   (cont), DIODE+V, uA/mA/A, VA = V+A with AC or DC; LowZ also draws the LowZ icon and AC or DC from
   args 3/4 (0x20008098/0x20008099); Hz/Ohm/cap via 0x08017858/0x080177f2/0x080177ac; A modes 20/21
   show m on range 0, VA 15/24 on range < 2. high [video: unit icons, confirmed] */

uint LCD_DrawUnits(char param_1,byte param_2,char param_3,uint param_4)

{
  DAT_2000804c = 0;
  if (param_1 == '\0') {
    DAT_2000779c = DAT_2000779c & 0xfffffffe;
    if ((param_4 & 0xff) == 0) {
      DAT_200077a4 = DAT_200077a4 & 0xfffffffe;
      DAT_20007794 = DAT_20007794 & 0xfffffffe;
    }
    else if (param_3 == '\0') {
      DAT_200077a4 = DAT_200077a4 | 1;
    }
    else {
      DAT_20007794 = DAT_20007794 | 1;
    }
    DAT_20007790 = DAT_20007790 | 0x40000000;
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\x01') {
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\x02') {
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\x03') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\x04') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\x06') {
    LCD_DrawHzUnits(param_2);
  }
  else if (param_1 == '\a') {
    DAT_200077a8 = DAT_200077a8 | 1;
  }
  else if (param_1 == '\b') {
    DAT_20007790 = DAT_20007790 | 0x20000;
  }
  else if (param_1 == '\t') {
    LCD_DrawOhmUnits(param_2);
  }
  else if (param_1 == '\n') {
    DAT_200077a0 = DAT_200077a0 | 0x80000000;
    DAT_20007798 = DAT_20007798 | 0x100;
  }
  else if (param_1 == '\v') {
    DAT_200077a8 = DAT_200077a8 | 0x80000000;
    DAT_200077a0 = DAT_200077a0 | 0x400;
  }
  else if (param_1 == '\f') {
    LCD_DrawCapUnits(param_2);
  }
  else if (param_1 == '\r') {
    DAT_20007790 = DAT_20007790 | 0x200;
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_20007794 = DAT_20007794 | 1;
  }
  else if (param_1 == '\x0e') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_20007794 = DAT_20007794 | 1;
  }
  else if (param_1 == '\x0f') {
    if (param_2 < 2) {
      DAT_20007798 = DAT_20007798 | 0x400;
    }
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_20007794 = DAT_20007794 | 1;
  }
  else if (param_1 == '\x10') {
    DAT_20007790 = DAT_20007790 | 0x200;
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x11') {
    DAT_20007790 = DAT_20007790 | 0x200;
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x12') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x13') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x14') {
    if (param_2 == 0) {
      DAT_20007798 = DAT_20007798 | 0x400;
    }
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x15') {
    if (param_2 == 0) {
      DAT_20007798 = DAT_20007798 | 0x400;
    }
    DAT_200077a8 = DAT_200077a8 | 0x400;
  }
  else if (param_1 == '\x16') {
    DAT_20007790 = DAT_20007790 | 0x200;
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_200077a4 = DAT_200077a4 | 1;
  }
  else if (param_1 == '\x17') {
    DAT_20007798 = DAT_20007798 | 0x400;
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_200077a4 = DAT_200077a4 | 1;
  }
  else if (param_1 == '\x18') {
    if (param_2 < 2) {
      DAT_20007798 = DAT_20007798 | 0x400;
    }
    DAT_200077a0 = DAT_200077a0 | 0x400;
    DAT_200077a8 = DAT_200077a8 | 0x400;
    DAT_200077a4 = DAT_200077a4 | 1;
  }
  return param_4;
}


// ==== LCD_DrawVaMilliIcon @ 08017700

/* (mode, range) VA A-position modes 24/15: "m" icon (COM1 SEG10) on for range < 2, off otherwise;
   VA_ApplyRange. Never executed. medium */

void LCD_DrawVaMilliIcon(char param_1,byte param_2)

{
  if ((param_1 == '\x18') || (param_1 == '\x0f')) {
    if (param_2 < 2) {
      DAT_20007798 = DAT_20007798 | 0x400;
    }
    else {
      DAT_20007798 = DAT_20007798 & 0xfffffbff;
    }
  }
  return;
}


// ==== LogView_DrawStatic @ 0801772e

/* entering the recall view: decimals 0x20007fed from Display_DecimalPlaces(view mode/range
   0x20007feb/fec), LCD_DrawIcons, units of the record's mode (flags 0x20007fe5/e6), 08019364,
   LCD_Flush. Never executed. high */

undefined4 LogView_DrawStatic(void)

{
  undefined4 unaff_r7;
  
  DAT_20007fed = Display_DecimalPlaces(DAT_20007feb,DAT_20007fec);
  LCD_DrawIcons();
  LCD_ClearUnits();
  LCD_DrawUnits(DAT_20007feb,DAT_20007fec,DAT_20007fe5,DAT_20007fe6);
  Bar_Off();
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_RedrawUnitPrefix @ 0801776e

/* LCD_RefreshDirty flag 0x20008061: unit prefixes for Hz (6), Ohm (9), cap (12). high */

undefined4 LCD_RedrawUnitPrefix(void)

{
  undefined4 unaff_r7;
  
  DAT_20008061 = 0;
  if (g_mode == '\x06') {
    LCD_DrawHzUnits(g_range);
  }
  else if (g_mode == '\t') {
    LCD_DrawOhmUnits(g_range);
  }
  else if (g_mode == '\f') {
    LCD_DrawCapUnits(g_range);
  }
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_DrawCapUnits @ 080177ac

/* capacitance unit prefix by range: range < 2 -> 0x20007798 bit 9 (likely n), else 0x20007790 bit 9
   (likely u); always F (0x200077a0 bit 9). From LCD_DrawUnits and the prefix redraw 0801776e.
   medium */

void LCD_DrawCapUnits(byte param_1)

{
  if (param_1 < 2) {
    DAT_20007790 = DAT_20007790 & 0xfffffdff;
    DAT_20007798 = DAT_20007798 | 0x200;
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x200;
    DAT_20007798 = DAT_20007798 & 0xfffffdff;
  }
  DAT_200077a0 = DAT_200077a0 | 0x200;
  return;
}


// ==== LCD_DrawOhmUnits @ 080177f2

/* resistance unit prefix by range: 0-1 none, 2-4 k (0x20007790 bit 8), 5+ M (bit 11); always OHM
   (0x20007798 bit 8). medium */

void LCD_DrawOhmUnits(byte param_1)

{
  if (param_1 < 5) {
    if (param_1 < 2) {
      DAT_20007790 = DAT_20007790 & 0xfffff6ff;
    }
    else {
      DAT_20007790 = DAT_20007790 & 0xfffff7ff | 0x100;
    }
  }
  else {
    DAT_20007790 = DAT_20007790 & 0xfffffeff | 0x800;
  }
  DAT_20007798 = DAT_20007798 | 0x100;
  return;
}


// ==== LCD_DrawHzUnits @ 08017858

/* frequency unit prefix by range: 0-1 none, 2-4 k (0x20007790 bit 8), 5+ M (bit 11); always Hz
   (0x200077a0 bit 8). medium */

void LCD_DrawHzUnits(byte param_1)

{
  if (param_1 < 5) {
    if (param_1 < 2) {
      DAT_20007790 = DAT_20007790 & 0xfffff6ff;
    }
    else {
      DAT_20007790 = DAT_20007790 & 0xfffff7ff | 0x100;
    }
  }
  else {
    DAT_20007790 = DAT_20007790 & 0xfffffeff | 0x800;
  }
  DAT_200077a0 = DAT_200077a0 | 0x100;
  return;
}


// ==== LCD_DrawSub @ 0801797c

/* sub display by g_setup_page: 0 temperature (0x20007fcc), 1 battery, 2 APO, 3 buzzer, 4 contrast,
   5..7 date/time (live or edit copy by 0x2000806e), 8 meter ID, 9 log interval, 10/11/12 mode pages
   (ACV: 10 Hz, 11 dBm; current: bdOn/bdOFF and burden voltage; VA: V or I half, bd flag, burden;
   diode 15/3 V; continuity mode), 100 = logging count 0x20007d38; overrides for message states
   0x2000801b/0x2000801d and cal mode (0x20008097 countdown). high (emulated: ACV page 10 '1.000',
   11 '8.239') */

undefined4 LCD_DrawSub(void)

{
  uint uVar1;
  int iVar2;
  undefined4 in_r3;
  undefined1 uVar3;
  
  DAT_20008062 = 0;
  if (DAT_2000801d == '\0') {
    if (DAT_2000801b == '\0') {
      if (g_setup_page == 'd') {
        if (DAT_20007fe1 == '\0') {
          LCD_DrawSubValue(DAT_20007f9c,0,0);
        }
        else {
          if (DAT_20007d38 < 100000) {
            uVar3 = 0;
            DAT_200077a0 = DAT_200077a0 & 0xfffdffff;
            uVar1 = DAT_20007d38;
          }
          else {
            uVar3 = 2;
            uVar1 = DAT_20007d38 / 10;
            DAT_200077a0 = DAT_200077a0 | 0x20000;
          }
          LCD_DrawSubValue(uVar1,uVar3,0);
        }
      }
      else if ((DAT_2000806e & (DAT_2000806f ^ 1)) == 0) {
        if ((g_cal_mode == '\0') || (DAT_20008097 != '\0')) {
          if ((g_cal_mode != '\0') && (DAT_20008097 != '\0')) {
            DAT_20008097 = DAT_20008097 + -1;
            g_setup_page = '\0';
          }
          if (g_setup_page == '\f') {
            if ((((g_mode == '\r') || (g_mode == '\x0e')) ||
                ((g_mode == '\x0f' ||
                 (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))))) &&
               (g_va_burden_display != '\0')) {
              if (DAT_200080c7 == '\0') {
                iVar2 = DAT_20007eb0 * 100;
              }
              else {
                iVar2 = DAT_20007eb0 * 10;
              }
              LCD_SubBurden(iVar2,DAT_200080c7);
            }
            else if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
                    (DAT_200080a5 != '\0')) {
              LCD_SubFrequency();
            }
          }
          else if (g_setup_page == '\v') {
            if (g_mode == '\x02') {
              LCD_SubDbm();
            }
            else if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
                    (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
              LCD_SubBurdenOnOff(DAT_200080a9);
            }
            else if ((((g_mode == '\x10') || (g_mode == '\x12')) ||
                     ((g_mode == '\x14' ||
                      (((g_mode == '\x11' || (g_mode == '\x13')) || (g_mode == '\x15')))))) &&
                    (DAT_200080a5 != '\0')) {
              LCD_SubBurden(DAT_20007e6c,100);
            }
            else if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
                    (DAT_200080a5 == '\0')) {
              LCD_SubFrequency();
            }
          }
          else if (g_setup_page == '\n') {
            if ((((g_mode == '\r') || (g_mode == '\x0e')) || (g_mode == '\x0f')) ||
               (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))) {
              if (DAT_200080c2 == '\0') {
                VA_SubDecimalsVolts();
                if (DAT_200080c0 == '\0') {
                  if ((DAT_200080ca == '\0') && (DAT_200080cb == '\0')) {
                    LCD_DrawSubVa(DAT_20007eac);
                  }
                  else {
                    LCD_ShowOFL(1);
                  }
                }
                else {
                  LCD_ShowDashes(1);
                }
              }
              else {
                VA_SubDecimalsAmps();
                if (DAT_200080c1 == '\0') {
                  if ((DAT_200080cb == '\0') && (DAT_200080ca == '\0')) {
                    LCD_DrawSubVa(DAT_20007eb0);
                  }
                  else {
                    LCD_ShowOFL(1);
                  }
                }
                else {
                  LCD_ShowDashes(1);
                }
              }
            }
            else if (g_mode == '\v') {
              LCD_SubDiodeVolts();
            }
            else if (g_mode == '\n') {
              LCD_SubContinuity(g_continuity_mode);
            }
            else if (((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
                     ((g_mode == '\x11' || (g_mode == '\x13')))) || (g_mode == '\x15')) {
              LCD_SubBurdenOnOff(DAT_200080a6);
            }
            else if (((g_mode == '\x02') && (DAT_200080ba == '\0')) || (g_mode == '\x04')) {
              LCD_SubFrequency();
            }
            else {
              g_setup_page = '\0';
              LCD_SubTemperature((int)DAT_20007fcc);
            }
          }
          else if (g_setup_page == '\x04') {
            LCD_SubContrast(g_lcd_contrast);
          }
          else if (g_setup_page == '\a') {
            if (DAT_2000806e == 0) {
              LCD_SubHourMinute(0,DAT_200080da,DAT_200080db);
            }
            else {
              LCD_SubHourMinute(0,DAT_200080e0,DAT_200080e1);
            }
          }
          else if (g_setup_page == '\x06') {
            if (DAT_2000806e == 0) {
              LCD_SubMonthDay(0,DAT_200080d8,DAT_200080d9);
            }
            else {
              LCD_SubMonthDay(0,DAT_200080de,DAT_200080df);
            }
          }
          else if (g_setup_page == '\x05') {
            if (DAT_2000806e == 0) {
              LCD_SubYear(0,DAT_200080d7);
            }
            else {
              LCD_SubYear(0,DAT_200080dd);
            }
          }
          else if (g_setup_page == '\b') {
            LCD_SubMeterId(1);
          }
          else if (g_setup_page == '\t') {
            if (DAT_2000806e == 0) {
              LCD_SubLogInterval(g_log_interval);
            }
            else {
              LCD_SubLogInterval(DAT_20007fa6);
            }
          }
          else if (g_setup_page == '\x02') {
            LCD_SubApo(g_apo_off);
          }
          else if (g_setup_page == '\x03') {
            LCD_SubBuzzer(g_buzzer_enabled);
          }
          else if (g_setup_page == '\x01') {
            DAT_20007798 = DAT_20007798 | 0x20000;
            LCD_SubBattery(g_battery_dV);
          }
          else {
            g_setup_page = '\0';
            LCD_SubTemperature((int)DAT_20007fcc);
          }
        }
        else {
          LCD_SubCal();
        }
      }
      else {
        LCD_SubEditBlink();
      }
    }
    else if (DAT_2000801b == '\x14') {
      LCD_SubLoBat();
    }
    else {
      LCD_SubSdError(DAT_2000801b);
    }
  }
  else {
    LCD_SubSdEnd();
  }
  return in_r3;
}


// ==== LCD_DrawSubValue @ 08017f28

/* (value, decimals, flush): sub display number: minus sign 0x200077a0 bit 29, LCD_SetSubDP, |value|
   to text drawn on the sub digits (08019ae0(1, ..)); flush now or set dirty 0x2000805b. Recall
   position, log count, many SETUP pages. high */

undefined4 LCD_DrawSubValue(int param_1,undefined1 param_2,char param_3,undefined4 param_4)

{
  if (param_1 < 0) {
    param_1 = -param_1;
    DAT_200077a0 = DAT_200077a0 | 0x20000000;
  }
  else {
    DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  }
  LCD_SetSubDP(param_2);
  Num_SplitDigits(param_1);
  Num_ToText(param_2);
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_3 == '\0') {
    DAT_2000805b = 1;
  }
  else {
    LCD_Flush();
  }
  return param_4;
}


// ==== LCD_SubFrequency @ 08017f94

/* sub frequency: LCD_SetSubFreqDP, LCD_DrawSubValue(0x20007d84 / 10, 0x20008040); ACV/ACmV page 10,
   AC current pages 11/12. high */

undefined4 LCD_SubFrequency(void)

{
  undefined4 unaff_r7;
  
  LCD_SetSubFreqDP();
  LCD_DrawSubValue(DAT_20007d84 / 10,DAT_20008040,0);
  return unaff_r7;
}


// ==== LCD_SetSubDP @ 08017fb4

/* (decimals 0..4) sub display decimal point in 0x20007790: clears bits 21/23/25/27, then 1 -> bit
   21, 2 -> 23, 3 -> 25, 4 -> 27. high */

void LCD_SetSubDP(byte param_1)

{
  DAT_20007790 = DAT_20007790 & 0xf55fffff;
  if (param_1 == 1) {
    DAT_20007790 = DAT_20007790 | 0x200000;
  }
  else if (param_1 != 0) {
    if (param_1 == 3) {
      DAT_20007790 = DAT_20007790 | 0x2000000;
    }
    else if (param_1 < 3) {
      DAT_20007790 = DAT_20007790 | 0x800000;
    }
    else if (param_1 == 4) {
      DAT_20007790 = DAT_20007790 | 0x8000000;
    }
  }
  return;
}


// ==== VA_SubDecimalsVolts @ 0801801e

/* VA sub display, voltage half: decimals 0x20008040 = 3 when V high 0x200080c6, else 4. high */

void VA_SubDecimalsVolts(void)

{
  if (DAT_200080c6 == '\0') {
    DAT_20008040 = 4;
  }
  else {
    DAT_20008040 = 3;
  }
  return;
}


// ==== VA_SubDecimalsAmps @ 0801803c

/* VA sub display, current half: decimals 0x20008040 = 4 for modes 13/22, 4 or 3 (I high 0x200080c7)
   for 14/23, 2 or 3 for 15/24 (sibling 0801801e is the V half: 4 / 3 by 0x200080c6). medium */

void VA_SubDecimalsAmps(void)

{
  if ((g_mode == '\r') || (g_mode == '\x16')) {
    DAT_20008040 = 4;
  }
  else if ((g_mode == '\x0e') || (g_mode == '\x17')) {
    if (DAT_200080c7 == '\0') {
      DAT_20008040 = 4;
    }
    else {
      DAT_20008040 = 3;
    }
  }
  else if (DAT_200080c7 == '\0') {
    DAT_20008040 = 2;
  }
  else {
    DAT_20008040 = 3;
  }
  return;
}


// ==== LCD_SetSubFreqDP @ 080180aa

/* Hz sub page (from 08017f94): decimals 0x20008040 by the frequency range 0x2000803e (08018102) for
   AC modes 2/4/16/18/20, else 0; LCD_SetSubDP. high */

undefined4 LCD_SetSubFreqDP(void)

{
  undefined4 unaff_r7;
  
  DAT_20008040 = 0;
  if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
     ((g_mode == '\x02' || (g_mode == '\x04')))) {
    DAT_20008040 = Freq_SubDecimals(DAT_2000803e);
  }
  LCD_SetSubDP(DAT_20008040);
  return unaff_r7;
}


// ==== Freq_SubDecimals @ 08018102

/* (freq range 0x2000803e) -> sub decimals: 0->2, 1->1, 2->3, 3->2, 4->1, else 0; LCD_SetSubFreqDP.
   high */

undefined1 Freq_SubDecimals(byte param_1)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if (param_1 == 0) {
    uVar1 = 2;
  }
  else if (param_1 == 2) {
    uVar1 = 3;
  }
  else if (param_1 < 2) {
    uVar1 = 1;
  }
  else if (param_1 == 4) {
    uVar1 = 1;
  }
  else if (param_1 < 4) {
    uVar1 = 2;
  }
  return uVar1;
}


// ==== LCD_SubEditBlink @ 0801813a

/* blink-off phase while editing a SETUP page (0x2000806e set, 0x2000806f clear): blanks the field
   being edited: pages 6/7 two spaces at digit 0 or 3 (by 0x200080d6), page 8 meter ID one digit (4
   - 0x20008071), else the whole sub; sub minus sign and DP off. high */

undefined4 LCD_SubEditBlink(void)

{
  undefined4 unaff_r7;
  
  DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  LCD_SetSubDP(0);
  if ((g_setup_page == '\a') || (g_setup_page == '\x06')) {
    if (DAT_200080d6 == '\0') {
      LCD_DrawText2(0,&DAT_0801840c);
    }
    else {
      LCD_DrawText2(3,&DAT_0801840c);
    }
  }
  else if (g_setup_page == '\b') {
    LCD_DrawCharAt('\x04' - DAT_20008071,&DAT_0801845c);
  }
  else {
    LCD_DrawText5(1,"     ");
  }
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_DrawSubVa @ 080181b6

/* (value) VA sub display (V 0x20007eac or I 0x20007eb0): LCD_SetSubDP(0x20008040), |value| to text
   on the sub, minus sign bit 29 after drawing; dirty 0x2000805b. high */

undefined4 LCD_DrawSubVa(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  
  if (param_1 < 0) {
    iVar1 = -param_1;
  }
  else {
    iVar1 = param_1;
  }
  LCD_SetSubDP(DAT_20008040);
  Num_SplitDigits(iVar1);
  Num_ToText(DAT_20008040);
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_1 < 0) {
    DAT_200077a0 = DAT_200077a0 | 0x20000000;
  }
  else {
    DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  }
  DAT_2000805b = 1;
  return param_4;
}


// ==== LCD_SubBurden @ 08018228

/* (value, scale): burden voltage on the sub display as "b" + 4 digits; scale 0 -> /1000 rounded,
   100 -> as is, else /100 rounded; result to 0x20007fca; "b OFL" when scale 100 and 0x200080a7
   (burden > 5000 counts); sub minus sign; 0x08018356 instead when 0x200080aa. high */

void LCD_SubBurden(int param_1,char param_2)

{
  int iVar1;
  bool bVar2;
  
  if (DAT_200080aa == '\0') {
    bVar2 = -1 < param_1;
    if (!bVar2) {
      param_1 = -param_1;
    }
    if (param_2 == 'd') {
      iVar1 = param_1;
    }
    else if (param_2 == '\0') {
      iVar1 = param_1 / 1000;
      if (499 < param_1 % 1000) {
        iVar1 = iVar1 + 1;
      }
    }
    else {
      iVar1 = param_1 / 100;
      if (0x31 < param_1 % 100) {
        iVar1 = iVar1 + 1;
      }
    }
    DAT_20007fca = (short)iVar1;
    if (!bVar2) {
      DAT_20007fca = -DAT_20007fca;
    }
    if ((param_2 == 'd') && (DAT_200080a7 != '\0')) {
      DAT_20007c31 = 0x20;
      DAT_20007c32 = 0x4f;
      DAT_20007c33 = 0x46;
      DAT_20007c34 = 0x4c;
    }
    else {
      Num_SplitDigits(iVar1);
      DAT_20008040 = 1;
      LCD_SetSubDP(1);
      Num_ToText(DAT_20008040);
    }
    if (bVar2) {
      DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
    }
    else {
      DAT_200077a0 = DAT_200077a0 | 0x20000000;
    }
    DAT_20007c30 = 0x62;
    LCD_DrawText5(1,&DAT_20007c30);
  }
  else {
    LCD_SubBurdnLabel();
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubBurdnLabel @ 08018356

/* sub display "burdn" (no DP), sets 0x2000805b (LCD dirty); LCD_SubBurden shows it instead of the
   value while countdown 0x200080aa (100, set on entering the burden page) runs. high */

undefined4 LCD_SubBurdnLabel(void)

{
  undefined4 unaff_r7;
  
  LCD_SetSubDP(0);
  DAT_20007c30 = 0x62;
  DAT_20007c31 = 0x75;
  DAT_20007c32 = 0x72;
  DAT_20007c33 = 100;
  DAT_20007c34 = 0x6e;
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubBattery @ 0801839a

/* (dV) SETUP page 1: "BAT" + 2 digits of g_battery_dV clamped to 99, sub DP 1 -> "BAT4.2";
   0x20008040 = 1; via Num digits 0x0801b090. high */

void LCD_SubBattery(ushort param_1)

{
  DAT_20008040 = 1;
  LCD_SetSubDP(1);
  if (99 < param_1) {
    param_1 = 99;
  }
  Num_SplitDigits(param_1);
  DAT_20007c30 = 0x42;
  DAT_20007c31 = 0x41;
  DAT_20007c32 = 0x54;
  DAT_20007c33 = (char)DAT_20007dbc + '0';
  DAT_20007c34 = (char)DAT_20007dc0 + '0';
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubContrast @ 08018410

/* (contrast 10..17) SETUP page 4: "Lcd-n" with n = contrast - 10 (char contrast + '&'). high */

void LCD_SubContrast(char param_1)

{
  LCD_SetSubDP(0);
  DAT_20007c30 = 0x4c;
  DAT_20007c31 = 99;
  DAT_20007c32 = 100;
  DAT_20007c33 = 0x2d;
  DAT_20007c34 = param_1 + '&';
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubApo @ 08018460

/* (g_apo_off) SETUP page 2: "APOoN" / "APOoF" with sub DP 2. high */

void LCD_SubApo(char param_1)

{
  LCD_SetSubDP(2);
  if (param_1 == '\0') {
    LCD_DrawText5(1,"APOoN");
  }
  else {
    LCD_DrawText5(1,"APOoF");
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubBuzzer @ 08018490

/* (g_buzzer_enabled) SETUP page 3: "b-On " / "b-OFF". high */

void LCD_SubBuzzer(char param_1)

{
  LCD_SetSubDP(0);
  if (param_1 == '\0') {
    LCD_DrawText5(1,"b-OFF");
  }
  else {
    LCD_DrawText5(1,"b-On ");
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubLogInterval @ 080184d4

/* (n) SETUP page 9: "In" + 3-digit right-justified number, leading blanks; live g_log_interval or
   edit copy 0x20007fa6. high */

undefined4 LCD_SubLogInterval(uint param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  LCD_SetSubDP(0);
  DAT_20007c30 = 0x49;
  DAT_20007c31 = 0x6e;
  if ((param_1 & 0xffff) < 100) {
    DAT_20007c32 = ' ';
    uVar1 = param_1;
  }
  else {
    DAT_20007c32 = (char)((param_1 & 0xffff) / 100) + '0';
    uVar1 = (param_1 & 0xffff) % 100;
  }
  if ((param_1 & 0xffff) < 10) {
    DAT_20007c33 = ' ';
  }
  else {
    uVar1 = uVar1 & 0xffff;
    DAT_20007c33 = (char)(uVar1 / 10) + '0';
  }
  DAT_20007c34 = (char)(uVar1 & 0xffff) + (char)((uVar1 & 0xffff) / 10) * -10 + '0';
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return param_4;
}


// ==== LCD_SubDbm @ 08018590  [DAT]

/* ACV SETUP page 11: dBm 0x20007e94 (float, computed by 0x0800a74c = 10*log10(V^2/0.6), 600-ohm
   reference) shown with 3/2/1/0 decimals by magnitude (<10, <100, <1000), sign on the sub minus;
   copy to 0x20007e98; " OFL" when 0x20008046. high (emulated: 2.0000 V -> '8.239') */

undefined4 LCD_SubDbm(void)

{
  int iVar1;
  undefined4 in_r3;
  uint uVar2;
  char cVar3;
  bool bVar4;
  
  cVar3 = '\x01';
  if (DAT_20008046 == '\0') {
    __aeabi_cfcmple(DAT_20007e94,0);
    if (cVar3 == '\0') {
      DAT_200077a0 = DAT_200077a0 | 0x20000000;
      cVar3 = '\x01';
      uVar2 = DAT_20007e94 ^ 0x80000000;
    }
    else {
      DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
      uVar2 = DAT_20007e94;
    }
    bVar4 = uVar2 == 0;
    __aeabi_cfrcmple(uVar2,0x447a0000);
    if ((bool)cVar3 && !bVar4) {
      bVar4 = uVar2 == 0;
      __aeabi_cfrcmple(uVar2,&DAT_42c80000);
      if ((bool)cVar3 && !bVar4) {
        bVar4 = uVar2 == 0;
        __aeabi_cfrcmple(uVar2,0x41200000);
        if ((bool)cVar3 && !bVar4) {
          DAT_20008040 = 3;
          __aeabi_fmul(0x447a0000,uVar2);
          iVar1 = __aeabi_f2uiz();
        }
        else {
          DAT_20008040 = 2;
          __aeabi_fmul(&DAT_42c80000,uVar2);
          iVar1 = __aeabi_f2uiz();
        }
      }
      else {
        DAT_20008040 = 1;
        __aeabi_fmul(0x41200000,uVar2);
        iVar1 = __aeabi_f2uiz();
      }
    }
    else {
      DAT_20008040 = 0;
      iVar1 = __aeabi_f2uiz(uVar2);
    }
    __aeabi_cfcmple(DAT_20007e94,0);
    if (cVar3 == '\0') {
      DAT_20007e98 = -iVar1;
    }
    else {
      DAT_20007e98 = iVar1;
    }
    LCD_SetSubDP(DAT_20008040);
    Num_SplitDigits(iVar1);
    Num_ToText(DAT_20008040);
    LCD_DrawText5(1,&DAT_20007c30);
    DAT_2000805b = 1;
  }
  else {
    LCD_ShowOFL(1);
  }
  return in_r3;
}


// ==== LCD_SubBurdenOnOff @ 080186ac

/* (flag) "bdOn " / "bdOFF" with sub DP 3: current page 10 (0x200080a6), VA page 11 (0x200080a9).
   high */

void LCD_SubBurdenOnOff(char param_1)

{
  LCD_SetSubDP(3);
  if (param_1 == '\0') {
    LCD_DrawText5(1,"bdOFF");
  }
  else {
    LCD_DrawText5(1,"bdOn ");
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubDiodeVolts @ 080186e0

/* diode page 10: "   15" (g_diode_15v) or "    3". high */

undefined4 LCD_SubDiodeVolts(void)

{
  undefined4 unaff_r7;
  
  LCD_SetSubDP(0);
  if (g_diode_15v == '\0') {
    LCD_DrawText5(1,"    3");
  }
  else {
    LCD_DrawText5(1,"   15");
  }
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubContinuity @ 0801871c

/* (g_continuity_mode) continuity page text: 0 "DN 30", 1 "UP 30", 2 "DN300", 3 "UP300"; clears sub
   minus. high */

void LCD_SubContinuity(byte param_1)

{
  DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  LCD_SetSubDP(0);
  if (param_1 == 0) {
    LCD_DrawText5(1,"DN 30");
  }
  else if (param_1 == 2) {
    LCD_DrawText5(1,"DN300");
  }
  else if (param_1 < 2) {
    LCD_DrawText5(1,"UP 30");
  }
  else if (param_1 == 3) {
    LCD_DrawText5(1,"UP300");
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_SubYear @ 08018788

/* (flush, year 0..99) SETUP page 5: 2000 + year as four digits; live 0x200080d7 or edit copy
   0x200080dd. high */

void LCD_SubYear(char param_1,uint param_2)

{
  LCD_SetSubDP(0);
  Num_SplitDigits((param_2 & 0xff) + 2000);
  Num_ToText(0);
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_1 == '\0') {
    DAT_2000805b = 1;
  }
  else {
    LCD_Flush();
  }
  return;
}


// ==== LCD_SubMonthDay @ 080187d8

/* (flush, month, day) SETUP page 6: "MM-DD" from 0x200080d8/d9 (live) or 0x200080de/df (edit); arg
   1 -> LCD_Flush else dirty flag. high */

void LCD_SubMonthDay(char param_1,undefined1 param_2,undefined1 param_3)

{
  LCD_SetSubDP(0);
  Num_SplitDigits(param_2);
  DAT_20007dbc = DAT_20007dbc + 0x30;
  DAT_20007c30 = (undefined1)DAT_20007dbc;
  DAT_20007dc0 = DAT_20007dc0 + 0x30;
  DAT_20007c31 = (undefined1)DAT_20007dc0;
  DAT_20007c32 = 0x2d;
  Num_SplitDigits(param_3);
  DAT_20007dbc = DAT_20007dbc + 0x30;
  DAT_20007c33 = (undefined1)DAT_20007dbc;
  DAT_20007dc0 = DAT_20007dc0 + 0x30;
  DAT_20007c34 = (undefined1)DAT_20007dc0;
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_1 == '\0') {
    DAT_2000805b = 1;
  }
  else {
    LCD_Flush();
  }
  return;
}


// ==== LCD_SubHourMinute @ 080188a0

/* (flush, hour, minute) SETUP page 7: "HH-MM", separator '-' or ' ' by blink phase 0x200080d5
   (toggled in main); 0x200080da/db live, 0x200080e0/e1 edit (0x0801896c picks). high */

void LCD_SubHourMinute(char param_1,undefined1 param_2,undefined1 param_3)

{
  LCD_SetSubDP(0);
  Num_SplitDigits(param_2);
  DAT_20007dbc = DAT_20007dbc + 0x30;
  DAT_20007c30 = (undefined1)DAT_20007dbc;
  DAT_20007dc0 = DAT_20007dc0 + 0x30;
  DAT_20007c31 = (undefined1)DAT_20007dc0;
  if (DAT_200080d5 == '\0') {
    DAT_20007c32 = 0x20;
  }
  else {
    DAT_20007c32 = 0x2d;
  }
  Num_SplitDigits(param_3);
  DAT_20007dbc = DAT_20007dbc + 0x30;
  DAT_20007c33 = (undefined1)DAT_20007dbc;
  DAT_20007dc0 = DAT_20007dc0 + 0x30;
  DAT_20007c34 = (undefined1)DAT_20007dc0;
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_1 == '\0') {
    DAT_2000805b = 1;
  }
  else {
    LCD_Flush();
  }
  return;
}


// ==== LCD_SubHourMinuteBlink @ 0801896c

/* (flush) TIM2: LCD_SubHourMinute with the live time 0x200080da/db or the edit copy 0x200080e0/e1
   (0x2000806e). medium */

void LCD_SubHourMinuteBlink(undefined1 param_1)

{
  if (DAT_2000806e == '\0') {
    LCD_SubHourMinute(param_1,DAT_200080da,DAT_200080db);
  }
  else {
    LCD_SubHourMinute(param_1,DAT_200080e0,DAT_200080e1);
  }
  return;
}


// ==== LCD_SubMeterId @ 080189b0

/* (flush) SETUP page 8: meter ID 0x20007d18 via Num_ToText(4). medium */

void LCD_SubMeterId(char param_1)

{
  LCD_SetSubDP(0);
  Num_SplitDigits(DAT_20007d18);
  Num_ToText(4);
  LCD_DrawText5(1,&DAT_20007c30);
  if (param_1 == '\0') {
    DAT_2000805b = 1;
  }
  else {
    LCD_Flush();
  }
  return;
}


// ==== LCD_SubTemperature @ 08018a00

/* (temp) SETUP page 0: converted temp (0x08018ad8) x10, Num_ToText(2), '-' placed, last char 'F'
   (g_temp_unit 0) or 'c' -> " 25.3c"; " OPEN" when 0x200080ae (only ever cleared: dead). high */

void LCD_SubTemperature(short param_1)

{
  short sVar1;
  bool bVar2;
  
  DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  sVar1 = Temp_ToDisplayTenths((int)param_1);
  if (DAT_200080ae == '\0') {
    LCD_SetSubDP(2);
    bVar2 = sVar1 < 0;
    if (bVar2) {
      sVar1 = -sVar1;
    }
    Num_SplitDigits(sVar1 * 10);
    Num_ToText(2);
    if (bVar2) {
      if (DAT_20007db4 == 0x20) {
        DAT_20007c31 = '-';
      }
      else {
        DAT_20007c30 = '-';
      }
    }
    if ((DAT_20007c30 == ' ') && (DAT_20007c30 = '\0', DAT_20007c31 == ' ')) {
      DAT_20007c31 = '\0';
    }
    if (g_temp_unit == '\0') {
      DAT_20007c34 = 0x46;
    }
    else {
      DAT_20007c34 = 99;
    }
    LCD_DrawText5(1,&DAT_20007c30);
    DAT_2000805b = 1;
  }
  else {
    LCD_SetSubDP(0);
    LCD_ShowOpen(1);
  }
  return;
}


// ==== Temp_ToDisplayTenths @ 08018ad8

/* (0.01 degC) -> 0.1 deg (degF unless g_temp_unit) for the sub display and BLE; BUG: rounds on the
   result's own last digit (x/10, +1 if (x/10) % 10 >= 5) instead of x % 10: 77.50 F -> 77.6
   (emulated). high */

undefined8
Temp_ToDisplayTenths(short param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  if (g_temp_unit == '\0') {
    param_1 = Temp_CToF((int)param_1);
  }
  param_1 = param_1 / 10;
  if (4 < (int)param_1 % 10) {
    param_1 = param_1 + 1;
  }
  return CONCAT44(param_4,(int)param_1);
}


// ==== Bar_RedrawDirty @ 08018b14

/* LCD_RefreshDirty flag 0x2000805e: Bar_Segments except Hz/pulse/duty, temp, cap. high */

undefined4 Bar_RedrawDirty(void)

{
  undefined4 unaff_r7;
  
  DAT_2000805e = 0;
  if ((((g_mode < 6) || (8 < g_mode)) && (g_mode != 0xc)) && (g_mode != 5)) {
    Bar_Segments();
  }
  return unaff_r7;
}


// ==== Bar_Segments @ 08018b80

/* lights bargraph segments 1..g_bar_count (0x20008041, 0..25) after Bar_Draw via 0x08018f14,
   clearing unlit ones with Bar_ClearRange(25, n) per segment (quadratic, harmless); called from
   0x08018b14 except Hz/pulse/duty/temp/cap. high [video: bargraph, confirmed] */

undefined4 Bar_Segments(void)

{
  undefined4 unaff_r7;
  
  Bar_DrawForMode();
  if (DAT_20008041 == 0) {
    Bar_ClearRange(0x19,1);
  }
  else {
    DAT_20007798 = DAT_20007798 | 2;
  }
  if (DAT_20008041 < 2) {
    Bar_ClearRange(0x19,2);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 2;
  }
  if (DAT_20008041 < 3) {
    Bar_ClearRange(0x19,3);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 2;
  }
  if (DAT_20008041 < 4) {
    Bar_ClearRange(0x19,4);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 4;
  }
  if (DAT_20008041 < 5) {
    Bar_ClearRange(0x19,5);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 4;
  }
  if (DAT_20008041 < 6) {
    Bar_ClearRange(0x19,6);
  }
  else {
    DAT_20007798 = DAT_20007798 | 4;
  }
  if (DAT_20008041 < 7) {
    Bar_ClearRange(0x19,7);
  }
  else {
    DAT_20007790 = DAT_20007790 | 4;
  }
  if (DAT_20008041 < 8) {
    Bar_ClearRange(0x19,8);
  }
  else {
    DAT_20007790 = DAT_20007790 | 8;
  }
  if (DAT_20008041 < 9) {
    Bar_ClearRange(0x19,9);
  }
  else {
    DAT_20007798 = DAT_20007798 | 8;
  }
  if (DAT_20008041 < 10) {
    Bar_ClearRange(0x19,10);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 8;
  }
  if (DAT_20008041 < 0xb) {
    Bar_ClearRange(0x19,0xb);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 8;
  }
  if (DAT_20008041 < 0xc) {
    Bar_ClearRange(0x19,0xc);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 0x10;
  }
  if (DAT_20008041 < 0xd) {
    Bar_ClearRange(0x19,0xd);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 0x10;
  }
  if (DAT_20008041 < 0xe) {
    Bar_ClearRange(0x19,0xe);
  }
  else {
    DAT_20007798 = DAT_20007798 | 0x10;
  }
  if (DAT_20008041 < 0xf) {
    Bar_ClearRange(0x19,0xf);
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x10;
  }
  if (DAT_20008041 < 0x10) {
    Bar_ClearRange(0x19,0x10);
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x20;
  }
  if (DAT_20008041 < 0x11) {
    Bar_ClearRange(0x19,0x11);
  }
  else {
    DAT_20007798 = DAT_20007798 | 0x20;
  }
  if (DAT_20008041 < 0x12) {
    Bar_ClearRange(0x19,0x12);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 0x20;
  }
  if (DAT_20008041 < 0x13) {
    Bar_ClearRange(0x19,0x13);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 0x20;
  }
  if (DAT_20008041 < 0x14) {
    Bar_ClearRange(0x19,0x14);
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 0x40;
  }
  if (DAT_20008041 < 0x15) {
    Bar_ClearRange(0x19,0x15);
  }
  else {
    DAT_200077a0 = DAT_200077a0 | 0x40;
  }
  if (DAT_20008041 < 0x16) {
    Bar_ClearRange(0x19,0x16);
  }
  else {
    DAT_20007798 = DAT_20007798 | 0x40;
  }
  if (DAT_20008041 < 0x17) {
    Bar_ClearRange(0x19,0x17);
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x40;
  }
  if (DAT_20008041 < 0x18) {
    Bar_ClearRange(0x19,0x18);
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x80;
  }
  if (DAT_20008041 < 0x19) {
    Bar_ClearRange(0x19,0x19);
  }
  else {
    DAT_20007798 = DAT_20007798 | 0x80;
  }
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== Bar_DrawForMode @ 08018f14

/* Bar_Draw(scale) per g_mode: 1 for LowZ, V/mV modes 1-4 on range 3, A modes 20/21 on range 2; 0
   for the other ranged modes; nothing for temp, Hz/pulse/duty, cap. medium */

undefined4 Bar_DrawForMode(void)

{
  uint uVar1;
  undefined4 unaff_r7;
  
  uVar1 = (uint)g_mode;
  if (uVar1 == 0) {
    Bar_Draw(1);
    return unaff_r7;
  }
  if (uVar1 - 1 < 4) {
    if (g_range == '\x03') {
      Bar_Draw(1);
      return unaff_r7;
    }
    Bar_Draw(0);
    return unaff_r7;
  }
  if (uVar1 - 9 < 3) {
LAB_08018f6e:
    Bar_Draw(0);
  }
  else {
    if (2 < uVar1 - 0xd) {
      if (uVar1 - 0x10 < 4) goto LAB_08018f6e;
      if (uVar1 - 0x14 < 2) {
        if (g_range == '\x02') {
          Bar_Draw(1);
          return unaff_r7;
        }
        Bar_Draw(0);
        return unaff_r7;
      }
      if (2 < uVar1 - 0x16) {
        return unaff_r7;
      }
    }
    Bar_Draw(0);
  }
  return unaff_r7;
}


// ==== Bar_Draw @ 08018fa8

/* bargraph drawing (calls Bar_ScaleMark). medium */

void Bar_Draw(char param_1)

{
  ushort uVar1;
  uint uVar2;
  
  if ((((g_mode < 6) || (8 < g_mode)) && (g_mode != 0xc)) && (g_mode != 5)) {
    if ((int)g_reading_fast < 0) {
      DAT_200077a0 = DAT_200077a0 | 1;
      DAT_20007798 = DAT_20007798 & 0xfffffffe;
    }
    else {
      DAT_200077a0 = DAT_200077a0 & 0xfffffffe;
      DAT_20007798 = DAT_20007798 | 1;
    }
    if (param_1 == '\x01') {
      DAT_20007790 = DAT_20007790 & 0xffffefff | 0x400;
      Bar_ScaleMark(1);
      uVar1 = 400;
    }
    else {
      DAT_20007790 = DAT_20007790 & 0xfffffbff | 0x1000;
      Bar_ScaleMark(0);
      uVar1 = 2000;
    }
    DAT_20007790 = DAT_20007790 | 2;
    if ((int)g_reading_fast < 0) {
      uVar2 = -g_reading_fast;
    }
    else {
      uVar2 = g_reading_fast;
    }
    DAT_20008041 = (byte)((int)uVar2 / (int)(uint)uVar1);
    if (DAT_20008046 != '\0') {
      if (DAT_2000804e == '\0') {
        DAT_20008041 = 0x19;
      }
      else if ((g_full_scale <= g_reading_fast) || ((int)g_reading_fast <= (int)-g_reading_fast)) {
        DAT_20008041 = 0x19;
      }
    }
    if ((g_mode == 0xb) && (0xf < DAT_20008041)) {
      DAT_20008041 = 0xf;
    }
  }
  return;
}


// ==== Bar_ScaleMark @ 0801912c

/* bargraph scale marks: COM2 SEG7 (normal) vs COM3 SEG8 (arg 1), then a range label (08019268).
   medium */

void Bar_ScaleMark(char param_1)

{
  uint uVar1;
  
  if (param_1 == '\x01') {
    DAT_20007798 = DAT_20007798 & 0xfffff7ff;
    DAT_200077a0 = DAT_200077a0 & 0xffffff7f;
    DAT_200077a8 = DAT_200077a8 & 0xffffff7f | 0x100;
    DAT_2000804d = 3;
    return;
  }
  DAT_200077a8 = DAT_200077a8 & 0xfffffeff;
  DAT_200077a0 = DAT_200077a0 | 0x80;
  DAT_2000804d = 0;
  uVar1 = (uint)g_mode;
  if (1 < uVar1 - 1) {
    if (uVar1 - 3 < 2) {
LAB_080191fe:
      Bar_RangeLabel(g_range + 1);
      return;
    }
    if (uVar1 == 9) {
      if (g_range < 2) {
        Bar_RangeLabel(g_range + 1);
        return;
      }
      if (g_range < 5) {
        Bar_RangeLabel(g_range - 2);
        return;
      }
      Bar_RangeLabel(g_range - 5);
      return;
    }
    if (uVar1 == 10) {
      Bar_RangeLabel(2);
      return;
    }
    if (uVar1 != 0xb) {
      if (uVar1 - 0x10 < 2) goto LAB_080191fe;
      if (1 < uVar1 - 0x12) {
        if (1 < uVar1 - 0x14) {
          DAT_2000804d = 0;
          return;
        }
        if (g_range == 0) {
          Bar_RangeLabel(2);
          return;
        }
        Bar_RangeLabel(0);
        return;
      }
    }
  }
  Bar_RangeLabel(g_range);
  return;
}


// ==== Bar_RangeLabel @ 08019268

/* (n) bargraph range label segments COM3 SEG7 + COM1 SEG11: 0 both off, 1 first, 2 both; state
   0x2000804d; n >= 3 leaves them. medium */

void Bar_RangeLabel(byte param_1)

{
  if (param_1 == 0) {
    DAT_200077a8 = DAT_200077a8 & 0xffffff7f;
    DAT_20007798 = DAT_20007798 & 0xfffff7ff;
    DAT_2000804d = 0;
  }
  else if (param_1 == 2) {
    DAT_200077a8 = DAT_200077a8 | 0x80;
    DAT_20007798 = DAT_20007798 | 0x800;
    DAT_2000804d = 2;
  }
  else if (param_1 < 2) {
    DAT_200077a8 = DAT_200077a8 | 0x80;
    DAT_20007798 = DAT_20007798 & 0xfffff7ff;
    DAT_2000804d = 1;
  }
  return;
}


// ==== Bar_Reset @ 080192fc

/* redraw path (0x08017222): ranged modes -> g_reading_fast = 0, bar count 0x20008041 = 0,
   Bar_DrawForMode, clear segments 25..1; Hz/pulse/duty/temp/cap -> Bar_Off. high */

undefined4 Bar_Reset(void)

{
  undefined4 unaff_r7;
  
  if ((((g_mode < 6) || (8 < g_mode)) && (g_mode != 0xc)) && (g_mode != 5)) {
    g_reading_fast = 0;
    DAT_20008041 = 0;
    Bar_DrawForMode();
    Bar_ClearRange(0x19,1);
    DAT_2000805b = 1;
  }
  else {
    Bar_Off();
  }
  return unaff_r7;
}


// ==== Bar_Off @ 08019364

/* clears every bargraph segment incl. bar0, +/- signs, scale marks and range labels
   (Bar_ClearRange(25,0)); also used by the memory-recall view. high */

undefined4 Bar_Off(void)

{
  undefined4 unaff_r7;
  
  DAT_20007790 = DAT_20007790 & 0xffffebff;
  DAT_200077a0 = DAT_200077a0 & 0xffffff7e;
  DAT_200077a8 = DAT_200077a8 & 0xfffffe7f;
  DAT_20007798 = DAT_20007798 & 0xfffff7fe;
  Bar_ClearRange(0x19,0);
  return unaff_r7;
}


// ==== Bar_ClearRange @ 08019408

/* (hi, lo): clears bargraph segments hi down to lo (fall-through switch; index 0 = the bar0 mark
   COM0 SEG1). high */

void Bar_ClearRange(byte param_1,char param_2)

{
  if (param_1 == 0) goto LAB_08019672;
  if (param_1 == 2) {
LAB_0801964e:
    DAT_200077a0 = DAT_200077a0 & 0xfffffffd;
    if (param_2 == '\x02') {
      return;
    }
  }
  else if (1 < param_1) {
    if (param_1 == 4) {
LAB_0801962a:
      DAT_200077a8 = DAT_200077a8 & 0xfffffffb;
      if (param_2 == '\x04') {
        return;
      }
    }
    else if (3 < param_1) {
      if (param_1 == 6) {
LAB_08019606:
        DAT_20007798 = DAT_20007798 & 0xfffffffb;
        if (param_2 == '\x06') {
          return;
        }
      }
      else if (5 < param_1) {
        if (param_1 == 8) {
LAB_080195e2:
          DAT_20007790 = DAT_20007790 & 0xfffffff7;
          if (param_2 == '\b') {
            return;
          }
        }
        else if (7 < param_1) {
          if (param_1 == 10) {
LAB_080195be:
            DAT_200077a0 = DAT_200077a0 & 0xfffffff7;
            if (param_2 == '\n') {
              return;
            }
          }
          else if (9 < param_1) {
            if (param_1 == 0xc) {
LAB_0801959a:
              DAT_200077a8 = DAT_200077a8 & 0xffffffef;
              if (param_2 == '\f') {
                return;
              }
            }
            else if (0xb < param_1) {
              if (param_1 == 0xe) {
LAB_08019576:
                DAT_20007798 = DAT_20007798 & 0xffffffef;
                if (param_2 == '\x0e') {
                  return;
                }
              }
              else if (0xd < param_1) {
                if (param_1 == 0x10) {
LAB_0801954e:
                  DAT_20007790 = DAT_20007790 & 0xffffffdf;
                  if (param_2 == '\x10') {
                    return;
                  }
                }
                else if (0xf < param_1) {
                  if (param_1 == 0x12) {
LAB_08019524:
                    DAT_200077a0 = DAT_200077a0 & 0xffffffdf;
                    if (param_2 == '\x12') {
                      return;
                    }
                  }
                  else if (0x11 < param_1) {
                    if (param_1 == 0x14) {
LAB_080194f4:
                      DAT_200077a8 = DAT_200077a8 & 0xffffffbf;
                      if (param_2 == '\x14') {
                        return;
                      }
                    }
                    else if (0x13 < param_1) {
                      if (param_1 == 0x16) {
LAB_080194c4:
                        DAT_20007798 = DAT_20007798 & 0xffffffbf;
                        if (param_2 == '\x16') {
                          return;
                        }
                      }
                      else if (0x15 < param_1) {
                        if (param_1 == 0x18) {
LAB_08019494:
                          DAT_20007790 = DAT_20007790 & 0xffffff7f;
                          if (param_2 == '\x18') {
                            return;
                          }
                        }
                        else if (0x17 < param_1) {
                          if (param_1 != 0x19) {
                            return;
                          }
                          DAT_20007798 = DAT_20007798 & 0xffffff7f;
                          if (param_2 == '\x19') {
                            return;
                          }
                          goto LAB_08019494;
                        }
                        DAT_20007790 = DAT_20007790 & 0xffffffbf;
                        if (param_2 == '\x17') {
                          return;
                        }
                        goto LAB_080194c4;
                      }
                      DAT_200077a0 = DAT_200077a0 & 0xffffffbf;
                      if (param_2 == '\x15') {
                        return;
                      }
                      goto LAB_080194f4;
                    }
                    DAT_200077a8 = DAT_200077a8 & 0xffffffdf;
                    if (param_2 == '\x13') {
                      return;
                    }
                    goto LAB_08019524;
                  }
                  DAT_20007798 = DAT_20007798 & 0xffffffdf;
                  if (param_2 == '\x11') {
                    return;
                  }
                  goto LAB_0801954e;
                }
                DAT_20007790 = DAT_20007790 & 0xffffffef;
                if (param_2 == '\x0f') {
                  return;
                }
                goto LAB_08019576;
              }
              DAT_200077a0 = DAT_200077a0 & 0xffffffef;
              if (param_2 == '\r') {
                return;
              }
              goto LAB_0801959a;
            }
            DAT_200077a8 = DAT_200077a8 & 0xfffffff7;
            if (param_2 == '\v') {
              return;
            }
            goto LAB_080195be;
          }
          DAT_20007798 = DAT_20007798 & 0xfffffff7;
          if (param_2 == '\t') {
            return;
          }
          goto LAB_080195e2;
        }
        DAT_20007790 = DAT_20007790 & 0xfffffffb;
        if (param_2 == '\a') {
          return;
        }
        goto LAB_08019606;
      }
      DAT_200077a0 = DAT_200077a0 & 0xfffffffb;
      if (param_2 == '\x05') {
        return;
      }
      goto LAB_0801962a;
    }
    DAT_200077a8 = DAT_200077a8 & 0xfffffffd;
    if (param_2 == '\x03') {
      return;
    }
    goto LAB_0801964e;
  }
  DAT_20007798 = DAT_20007798 & 0xfffffffd;
  if (param_2 == '\x01') {
    return;
  }
LAB_08019672:
  DAT_20007790 = DAT_20007790 & 0xfffffffd;
  return;
}


// ==== LCD_CalPrompt @ 08019698

/* (kind) main "-X-nn" calibration prompt, X = O/G/4/5/3 for 0..4, nn = step 0x20008095; BUG: units
   digit = nn - 10 (not nn % 10), so nn >= 20 garbles. high */

undefined4 LCD_CalPrompt(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  DAT_20007c30 = 0x2d;
  if (param_1 == 0) {
    DAT_20007c31 = 0x4f;
  }
  else if (param_1 == 2) {
    DAT_20007c31 = 0x34;
  }
  else if (param_1 < 2) {
    DAT_20007c31 = 0x47;
  }
  else if (param_1 == 4) {
    DAT_20007c31 = 0x33;
  }
  else if (param_1 < 4) {
    DAT_20007c31 = 0x35;
  }
  DAT_20007c32 = 0x2d;
  if (DAT_20008095 < 10) {
    DAT_20007c33 = '0';
    bVar1 = DAT_20008095;
  }
  else {
    DAT_20007c33 = DAT_20008095 / 10 + 0x30;
    bVar1 = DAT_20008095 - 10;
  }
  DAT_20007c34 = bVar1 + 0x30;
  LCD_DrawText5(0,&DAT_20007c30);
  DAT_2000805b = 1;
  return param_4;
}


// ==== LCD_CalEndText @ 08019738

/* (kind) main calibration end text: 0 "OFF-E", 1 "G-END", 2 "400-E", 3 "5KEND"; clears main minus.
   high */

void LCD_CalEndText(byte param_1)

{
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  if (param_1 == 0) {
    LCD_DrawText5(0,"OFF-E");
  }
  else if (param_1 == 2) {
    LCD_DrawText5(0,"400-E");
  }
  else if (param_1 < 2) {
    LCD_DrawText5(0,"G-END");
  }
  else if (param_1 == 3) {
    LCD_DrawText5(0,"5KEND");
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_MainSdCal @ 08019792

/* boot SD state 1: main "Sd.CAL" (DP 3, minus off), flush. high */

undefined4 LCD_MainSdCal(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0,"SdCAL");
  LCD_SetMainDP(3);
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_MainSdSave @ 080197b6

/* boot SD state 2: main "Sd.SAv", flush. high */

undefined4 LCD_MainSdSave(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0,"SdSAv");
  LCD_SetMainDP(3);
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_MainSdSet @ 080197e0

/* boot SD state 2 (0x20008089): main "Sd.SEt", flush. medium */

undefined4 LCD_MainSdSet(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0,"SdSEt");
  LCD_SetMainDP(3);
  LCD_Flush();
  return unaff_r7;
}


// ==== Show_SAvEd @ 08019808

/* "SAvEd" (shown after a cal save AND a cal load: same routine). high (emulated) */

undefined4 Show_SAvEd(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0,"SAvEd");
  LCD_SetMainDP(0);
  LCD_Flush();
  return unaff_r7;
}


// ==== Show_Err @ 0801982c

/* "-Err-" (cal save/load failed: e.g. 9 = EEPROM read-back mismatch). high (emulated) */

undefined4 Show_Err(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0,"-Err-");
  LCD_SetMainDP(0);
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_MainCal1 @ 08019854

/* boot calibration entry: main " CAL1" (minus off), dirty. high */

undefined4 LCD_MainCal1(void)

{
  undefined4 unaff_r7;
  
  DAT_2000779c = DAT_2000779c & 0xffffff7f;
  LCD_DrawText5(0," CAL1");
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubCal @ 08019880

/* sub " CAL " (sub minus off, no DP); boot cal entry and LCD_DrawSub in cal mode. high */

undefined4 LCD_SubCal(void)

{
  undefined4 unaff_r7;
  
  DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  LCD_SetSubDP(0);
  LCD_DrawText5(1," CAL ");
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubLoBat @ 080198b0

/* sub "LoBAt" when log error 0x2000801b = 0x14 (Log_Start refused on low battery 0x2000803b). high
    */

undefined4 LCD_SubLoBat(void)

{
  undefined4 unaff_r7;
  
  DAT_200077a0 = DAT_200077a0 & 0xfffdffff;
  DAT_20007c30 = 0x4c;
  DAT_20007c31 = 0x6f;
  DAT_20007c32 = 0x42;
  DAT_20007c33 = 0x41;
  DAT_20007c34 = 0x74;
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubSdEnd @ 080198fc

/* sub "SdEnd" when 0x2000801d (logging ended by the 599999 limit in Log_Write). high */

undefined4 LCD_SubSdEnd(void)

{
  undefined4 unaff_r7;
  
  DAT_200077a0 = DAT_200077a0 & 0xfffdffff;
  DAT_20007c30 = 0x53;
  DAT_20007c31 = 100;
  DAT_20007c32 = 0x45;
  DAT_20007c33 = 0x6e;
  DAT_20007c34 = 100;
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return unaff_r7;
}


// ==== LCD_SubSdError @ 08019948

/* (code) sub "SdEnn", code 0x2000801b from Log_WriteRetry; same nn - 10 bug: right only for 0..19;
   low battery stop sets 100 -> "SdE" + two blanks. high */

undefined4 LCD_SubSdError(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  DAT_200077a0 = DAT_200077a0 & 0xfffdffff;
  DAT_20007c30 = 0x53;
  DAT_20007c31 = 100;
  DAT_20007c32 = 0x45;
  if (param_1 < 10) {
    DAT_20007c33 = '0';
  }
  else {
    DAT_20007c33 = param_1 / 10 + 0x30;
    param_1 = param_1 - 10;
  }
  DAT_20007c34 = param_1 + 0x30;
  LCD_DrawText5(1,&DAT_20007c30);
  DAT_2000805b = 1;
  return param_4;
}


// ==== LCD_ShowOFL @ 080199b4

/* (line 0 main / 1 sub) " OFL"; main shows " LEAD" instead when 0x20008076, main minus from
   0x20008045 (negative OL), sub minus cleared. high */

void LCD_ShowOFL(char param_1)

{
  if ((param_1 == '\0') && (DAT_20008076 != '\0')) {
    LCD_ShowLead(0);
  }
  else {
    if (param_1 == '\x01') {
      DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
    }
    else if (DAT_20008045 == '\0') {
      DAT_2000779c = DAT_2000779c & 0xffffff7f;
    }
    else {
      DAT_2000779c = DAT_2000779c | 0x80;
    }
    LCD_DrawText5(param_1," OFL ");
  }
  return;
}


// ==== LCD_ShowOpen @ 08019a22

/* (line) " OPEN", minus cleared, " LEAD" override on main; only caller LCD_SubTemperature (dead
   branch). high */

void LCD_ShowOpen(char param_1)

{
  if ((param_1 == '\0') && (DAT_20008076 != '\0')) {
    LCD_ShowLead(0);
  }
  else {
    if (param_1 == '\x01') {
      DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
    }
    else {
      DAT_2000779c = DAT_2000779c & 0xffffff7f;
    }
    LCD_DrawText5(param_1," OPEN");
  }
  return;
}


// ==== LCD_ShowLead @ 08019a74

/* (line) " LEAD"; LCD_DrawMain, LCD_ShowOFL, LCD_ShowOpen. high */

void LCD_ShowLead(undefined1 param_1)

{
  LCD_DrawText5(param_1," LEAD");
  return;
}


// ==== LCD_ShowDashes @ 08019a86

/* (line) "-----"; VA modes main, VA sub halves. high */

void LCD_ShowDashes(undefined1 param_1)

{
  LCD_DrawText5(param_1,"-----");
  return;
}


// ==== LCD_ShowBlank @ 08019a98

/* (line) five blanks, clears negative-OL flag 0x20008045 and that line's minus. high */

void LCD_ShowBlank(char param_1)

{
  DAT_20008045 = 0;
  if (param_1 == '\x01') {
    DAT_200077a0 = DAT_200077a0 & 0xdfffffff;
  }
  else {
    DAT_2000779c = DAT_2000779c & 0xffffff7f;
  }
  LCD_DrawText5(param_1,"     ");
  return;
}


// ==== LCD_DrawText5 @ 08019ae0

/* (line 0 main / 1 sub, text) five chars via LCD_DrawChar at positions 5..9 (main) or 0..4 (sub).
   high */

undefined4 LCD_DrawText5(char param_1,undefined1 *param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  char cVar2;
  
  bVar1 = 0;
  if (param_1 == '\0') {
    cVar2 = '\x05';
  }
  else {
    cVar2 = '\0';
  }
  while( true ) {
    LCD_DrawChar(*param_2,cVar2);
    bVar1 = bVar1 + 1;
    if (4 < bVar1) break;
    param_2 = param_2 + 1;
    cVar2 = cVar2 + '\x01';
  }
  return param_4;
}


// ==== LCD_DrawCharAt @ 08019b12

/* (position, text) one LCD_DrawChar; LCD_SubEditBlink. high */

undefined4
LCD_DrawCharAt(undefined1 param_1,undefined1 *param_2,undefined4 param_3,undefined4 param_4)

{
  LCD_DrawChar(*param_2,param_1);
  return param_4;
}


// ==== LCD_DrawText2 @ 08019b24

/* (position, text) two chars at position, position+1; LCD_SubEditBlink date fields. high */

undefined4 LCD_DrawText2(char param_1,undefined1 *param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  
  bVar1 = 0;
  while( true ) {
    LCD_DrawChar(*param_2,param_1);
    bVar1 = bVar1 + 1;
    if (1 < bVar1) break;
    param_2 = param_2 + 1;
    param_1 = param_1 + '\x01';
  }
  return param_4;
}


// ==== LCD_ClearUnits @ 08019b48

/* clears the main unit icons (OHM, n, m, Hz, F, V, BEEP, A, DIODE, k, u, M, %, LowZ and one more
   COM0 bit); ms kept while 1 ms PEAK (0x200080b7). high */

void LCD_ClearUnits(void)

{
  uint uVar1;
  
  DAT_20007798 = DAT_20007798 & 0xfffff8ff;
  DAT_200077a0 = DAT_200077a0 & 0x7ffff8ff;
  uVar1 = DAT_200077a8 & 0x7ffffbff;
  if (DAT_200080b7 == '\0') {
    uVar1 = DAT_200077a8 & 0x7ffffbfe;
  }
  DAT_200077a8 = uVar1;
  DAT_20007790 = DAT_20007790 & 0x9ffdf4ff;
  return;
}


// ==== LCD_DrawApoIcon @ 08019c5c

/* (flush) APO icon (COM2 SEG39) on when APO active (g_apo_off == 0), not logging, no MIN/MAX;
   untouched in the last 60 s (0x200080e6, blink owner). high [video: sub_8019c5d, confirmed] */

void LCD_DrawApoIcon(char param_1)

{
  if (DAT_200080e6 == '\0') {
    if ((g_apo_off == '\0' && DAT_20007fe1 == '\0') && (DAT_2000804e == '\0')) {
      DAT_200077a4 = DAT_200077a4 | 0x80;
    }
    else {
      DAT_200077a4 = DAT_200077a4 & 0xffffff7f;
    }
    if (param_1 == '\x01') {
      LCD_Flush();
    }
  }
  return;
}


// ==== LCD_BlinkApoIcon @ 08019cbc

/* main, last 60 s before auto power-off (g_apo_seconds < 60): APO icon (COM2 SEG39) follows the 1
   Hz phase 0x200080e7, flush: the APO icon blinks during the final minute. high */

undefined4 LCD_BlinkApoIcon(void)

{
  undefined4 unaff_r7;
  
  if (DAT_200080e7 == '\0') {
    DAT_200077a4 = DAT_200077a4 & 0xffffff7f;
  }
  else {
    DAT_200077a4 = DAT_200077a4 | 0x80;
  }
  LCD_Flush();
  return unaff_r7;
}


// ==== LCD_DrawLowBatIcon @ 08019cf0

/* (flush) low-battery icon (COM3 SEG39) from 0x2000803b; Battery_Update, LCD_DrawIcons. high
   [video: sub_8019cf1 IC_X_3_39, identified here] */

void LCD_DrawLowBatIcon(char param_1)

{
  if (DAT_2000803b == '\0') {
    DAT_200077ac = DAT_200077ac & 0xffffff7f;
  }
  else {
    DAT_200077ac = DAT_200077ac | 0x80;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawBtIcon @ 08019d2c

/* (flush) BT icon (COM0 SEG31) from g_ble_on; BLE_Toggle, LCD_DrawIcons. high [video: sub_8019d2d,
   confirmed] */

void LCD_DrawBtIcon(char param_1)

{
  if (g_ble_on == '\0') {
    DAT_20007790 = DAT_20007790 & 0x7fffffff;
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x80000000;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawAutoIcon @ 08019d68

/* (flush) AUTO icon (COM3 SEG32) from Range_IsAuto. high [video: sub_8019d69, confirmed] */

void LCD_DrawAutoIcon(char param_1)

{
  int iVar1;
  
  iVar1 = Range_IsAuto();
  if (iVar1 == 0) {
    DAT_200077ac = DAT_200077ac & 0xfffffffe;
  }
  else {
    DAT_200077ac = DAT_200077ac | 1;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== Range_IsAuto @ 08019da2

/* 1 = AUTO range: not cal mode, not logging without auto-hold, mode not LowZ/temp/duty/cont/diode,
   manual flag 0x20008042 clear; used by the AUTO icon (0x08019d68) and BLE_BuildPacket. high */

undefined1 Range_IsAuto(void)

{
  undefined1 uVar1;
  
  if (g_cal_mode == '\0') {
    if ((DAT_20007fe1 & (DAT_20008050 ^ 1)) == 0) {
      if ((((g_mode == '\v') || (g_mode == '\n')) || (g_mode == '\x05')) ||
         ((g_mode == '\0' || (g_mode == '\b')))) {
        uVar1 = 0;
      }
      else if (DAT_20008042 == '\0') {
        uVar1 = 1;
      }
      else {
        uVar1 = 0;
      }
    }
    else {
      uVar1 = 0;
    }
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}


// ==== LCD_DrawMinMaxIcons @ 08019e24

/* (flush): MIN/MAX/AVG icons from 0x2000804e (1 MAX, 2 MIN, 3 AVG, 4 all three); in 1 ms PEAK
   (0x200080b7) MAX or MIN by 0x200080b8; none while logging (0x20007fe1) or 0x20007fe4; arg 1 ->
   LCD_Flush. high */

void LCD_DrawMinMaxIcons(char param_1)

{
  DAT_200077a8 = DAT_200077a8 & 0xbfffffff;
  DAT_200077a0 = DAT_200077a0 & 0xbfffffff;
  DAT_20007798 = DAT_20007798 & 0xbfffffff;
  if (DAT_20007fe1 == '\0' && DAT_20007fe4 == '\0') {
    if (DAT_200080b7 == '\0') {
      if (DAT_2000804e == 1) {
        DAT_200077a0 = DAT_200077a0 | 0x40000000;
      }
      else if (DAT_2000804e != 0) {
        if (DAT_2000804e == 3) {
          DAT_20007798 = DAT_20007798 | 0x40000000;
        }
        else if (DAT_2000804e < 3) {
          DAT_200077a8 = DAT_200077a8 | 0x40000000;
        }
        else if (DAT_2000804e == 4) {
          DAT_200077a8 = DAT_200077a8 | 0x40000000;
          DAT_200077a0 = DAT_200077a0 | 0x40000000;
          DAT_20007798 = DAT_20007798 | 0x40000000;
        }
      }
    }
    else if (DAT_200080b8 == '\0') {
      DAT_200077a0 = DAT_200077a0 | 0x40000000;
    }
    else {
      DAT_200077a8 = DAT_200077a8 | 0x40000000;
    }
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawRelIcon @ 08019f2e

/* (flush) REL icon (COM0 SEG0) from 0x20008058, off while logging or in recall view (0x20007fe4).
   high [video: sub_8019f2f, confirmed] */

void LCD_DrawRelIcon(char param_1)

{
  if (DAT_20007fe1 == '\0') {
    if (DAT_20007fe4 == '\0') {
      if (DAT_20008058 == '\0') {
        DAT_20007790 = DAT_20007790 & 0xfffffffe;
      }
      else {
        DAT_20007790 = DAT_20007790 | 1;
      }
    }
    else {
      DAT_20007790 = DAT_20007790 & 0xfffffffe;
    }
  }
  else {
    DAT_20007790 = DAT_20007790 & 0xfffffffe;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawHoldIcon @ 08019fa2

/* (flush) Hold_State 1 -> "A-" + HOLD, 2 -> HOLD, 0 -> neither (COM1/COM2 SEG16). high [video:
   sub_8019fa3, confirmed] */

undefined4 LCD_DrawHoldIcon(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  
  cVar1 = Hold_State();
  if (cVar1 == '\x01') {
    DAT_20007798 = DAT_20007798 | 0x10000;
    DAT_200077a0 = DAT_200077a0 | 0x10000;
  }
  else if (cVar1 == '\x02') {
    DAT_20007798 = DAT_20007798 & 0xfffeffff;
    DAT_200077a0 = DAT_200077a0 | 0x10000;
  }
  else {
    DAT_20007798 = DAT_20007798 & 0xfffeffff;
    DAT_200077a0 = DAT_200077a0 & 0xfffeffff;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return param_4;
}


// ==== Hold_State @ 0801a028

/* 0 none (also recall view, or logging without auto-hold), 2 HOLD (0x2000804f), 1 auto-hold
   (0x20008050); also BLE_BuildPacket status. high */

undefined1 Hold_State(void)

{
  undefined1 uVar1;
  
  if ((DAT_20007fe1 & (DAT_20008050 ^ 1)) == 0) {
    if (DAT_20007fe4 == '\0') {
      if (DAT_2000804f == '\0') {
        if (DAT_20008050 == 0) {
          uVar1 = 0;
        }
        else {
          uVar1 = 1;
        }
      }
      else {
        uVar1 = 2;
      }
    }
    else {
      uVar1 = 0;
    }
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}


// ==== LCD_DrawPeakIcon @ 0801a07c

/* (flush) "1ms" PEAK icon (COM3 SEG0+16) from 0x200080b7, or recorded flag 0x20007fe8 in recall
   view. high [video: sub_801a07d, confirmed] */

void LCD_DrawPeakIcon(char param_1)

{
  if (DAT_20007fe4 == '\0') {
    if (DAT_200080b7 == '\0') {
      DAT_200077a8 = DAT_200077a8 & 0xfffefffe;
    }
    else {
      DAT_200077a8 = DAT_200077a8 | 0x10001;
    }
  }
  else if (DAT_20007fe8 == '\0') {
    DAT_200077a8 = DAT_200077a8 & 0xfffefffe;
  }
  else {
    DAT_200077a8 = DAT_200077a8 | 0x10001;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawLpfIcon @ 0801a110

/* 1kHz LPF icon (COM1 SEG31) from g_lpf_1khz, or recorded flag 0x20007fe7 in recall view. high
   [video: sub_801a111, confirmed] */

void LCD_DrawLpfIcon(void)

{
  if (DAT_20007fe4 == '\0') {
    if (g_lpf_1khz == '\0') {
      DAT_20007798 = DAT_20007798 & 0x7fffffff;
    }
    else {
      DAT_20007798 = DAT_20007798 | 0x80000000;
    }
  }
  else if (DAT_20007fe7 == '\0') {
    DAT_20007798 = DAT_20007798 & 0x7fffffff;
  }
  else {
    DAT_20007798 = DAT_20007798 | 0x80000000;
  }
  return;
}


// ==== LCD_DrawAcDc @ 0801a17c

/* AC / DC icons (and COM1 SEG32, likely the "+" of AC+DC) by mode: AC for 2/4/16/18/20, DC for
   1/3/17/19/21, all three with AC+DC 0x200080ba; in recall/view state 0x20007fe4 uses the saved
   mode 0x20007feb and flag 0x20007fe9. high */

void LCD_DrawAcDc(void)

{
  if (DAT_20007fe4 == '\0') {
    if (DAT_200080ba == '\0') {
      DAT_200077a4 = DAT_200077a4 & 0xfffffffe;
      DAT_2000779c = DAT_2000779c & 0xfffffffe;
      DAT_20007794 = DAT_20007794 & 0xfffffffe;
      if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
         ((g_mode == '\x02' || (g_mode == '\x04')))) {
        DAT_20007794 = DAT_20007794 | 1;
      }
      if (((g_mode == '\x11') || (g_mode == '\x13')) ||
         ((g_mode == '\x15' || ((g_mode == '\x01' || (g_mode == '\x03')))))) {
        DAT_200077a4 = DAT_200077a4 | 1;
      }
    }
    else {
      DAT_200077a4 = DAT_200077a4 | 1;
      DAT_2000779c = DAT_2000779c | 1;
      DAT_20007794 = DAT_20007794 | 1;
    }
  }
  else if (DAT_20007fe9 == '\0') {
    DAT_200077a4 = DAT_200077a4 & 0xfffffffe;
    DAT_2000779c = DAT_2000779c & 0xfffffffe;
    DAT_20007794 = DAT_20007794 & 0xfffffffe;
    if ((((DAT_20007feb == '\x10') || (DAT_20007feb == '\x12')) || (DAT_20007feb == '\x14')) ||
       ((DAT_20007feb == '\x02' || (DAT_20007feb == '\x04')))) {
      DAT_20007794 = DAT_20007794 | 1;
    }
    if (((DAT_20007feb == '\x11') || (DAT_20007feb == '\x13')) ||
       ((DAT_20007feb == '\x15' || ((DAT_20007feb == '\x01' || (DAT_20007feb == '\x03')))))) {
      DAT_200077a4 = DAT_200077a4 | 1;
    }
  }
  else {
    DAT_200077a4 = DAT_200077a4 | 1;
    DAT_2000779c = DAT_2000779c | 1;
    DAT_20007794 = DAT_20007794 | 1;
  }
  return;
}


// ==== LCD_DrawMemIcon @ 0801a32a

/* (flush) MEM icon (COM0 SEG16) when logging with 0x20007fe3 or in recall view 0x20007fe4;
   Log_Start/Stop/Write callers. high [video: sub_801a32b, confirmed] */

void LCD_DrawMemIcon(char param_1)

{
  if ((DAT_20007fe1 & DAT_20007fe3) == 0 && DAT_20007fe4 == '\0') {
    DAT_20007790 = DAT_20007790 & 0xfffeffff;
  }
  else {
    DAT_20007790 = DAT_20007790 | 0x10000;
  }
  if (param_1 == '\x01') {
    LCD_Flush();
  }
  return;
}


// ==== LCD_DrawHvIcon @ 0801a370

/* high-voltage icon (COM0 SEG39, code 0x27, not in the icon tables) from 0x200080b1;
   Display_VoltageWarning, LCD_DrawIcons. medium */

void LCD_DrawHvIcon(void)

{
  if (DAT_200080b1 == '\0') {
    DAT_20007794 = DAT_20007794 & 0xffffff7f;
  }
  else {
    DAT_20007794 = DAT_20007794 | 0x80;
  }
  return;
}


// ==== LCD_DrawTempUnitIcon @ 0801a3a4

/* temp unit icon in mode 5 only: g_temp_unit 0 -> COM3 SEG11, else COM2 SEG11; both off in other
   modes. high [video: sub_801a3a5, confirmed] */

void LCD_DrawTempUnitIcon(void)

{
  if (g_mode == '\x05') {
    if (g_temp_unit == '\0') {
      DAT_200077a0 = DAT_200077a0 & 0xfffff7ff;
      DAT_200077a8 = DAT_200077a8 | 0x800;
    }
    else {
      DAT_200077a0 = DAT_200077a0 | 0x800;
      DAT_200077a8 = DAT_200077a8 & 0xfffff7ff;
    }
  }
  else {
    DAT_200077a0 = DAT_200077a0 & 0xfffff7ff;
    DAT_200077a8 = DAT_200077a8 & 0xfffff7ff;
  }
  return;
}


// ==== LCD_DrawSubUnits @ 0801a440

/* sub-display unit icons per SETUP page and mode (page 1 V, page 11 ACV dBm segments COM0 SEG18/19,
   Hz/kHz on frequency pages, burden pages, diode/VA via 0x0801a3a4/0x0801a6d8); skipped in cal mode
   and on page 100; also called from HY_OnCounter. medium */

undefined4 LCD_DrawSubUnits(void)

{
  undefined4 unaff_r7;
  
  if ((g_cal_mode == '\0') && (LCD_ClearSubUnits(), g_setup_page != 'd')) {
    if (((((g_mode == '\x10') || (((g_mode == '\x12' || (g_mode == '\x14')) || (g_mode == '\x11'))))
         || ((g_mode == '\x13' || (g_mode == '\x15')))) && (g_setup_page == '\v')) &&
       (DAT_200080a5 != '\0')) {
      DAT_20007790 = DAT_20007790 | 0x40000;
      DAT_20007798 = DAT_20007798 | 0x20000;
    }
    else if (g_setup_page == '\f') {
      if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
         (DAT_200080a5 != '\0')) {
        if (1 < DAT_2000803e) {
          DAT_200077a0 = DAT_200077a0 | 0x20000;
        }
        DAT_200077a8 = DAT_200077a8 | 0x20000;
      }
    }
    else if (g_setup_page == '\v') {
      if (g_mode == '\x02') {
        DAT_20007790 = DAT_20007790 | 0xc0000;
      }
      else if ((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) &&
              (DAT_200080a5 == '\0')) {
        if (1 < DAT_2000803e) {
          DAT_200077a0 = DAT_200077a0 | 0x20000;
        }
        DAT_200077a8 = DAT_200077a8 | 0x20000;
      }
    }
    else if (g_setup_page == '\n') {
      if (g_mode == '\v') {
        DAT_20007798 = DAT_20007798 | 0x20000;
        LCD_DrawTempUnitIcon();
      }
      else if (((g_mode == '\r') || (g_mode == '\x0e')) ||
              ((g_mode == '\x0f' ||
               (((g_mode == '\x16' || (g_mode == '\x17')) || (g_mode == '\x18')))))) {
        if (DAT_200080c2 == '\0') {
          DAT_20007798 = DAT_20007798 | 0x20000;
        }
        else {
          LCD_SubVaAmpsUnit();
        }
      }
      else if (g_mode == '\n') {
        DAT_200077a8 = DAT_200077a8 | 0x40000;
      }
      else if ((g_mode == '\x04') || ((g_mode == '\x02' && (DAT_200080ba == '\0')))) {
        if (1 < DAT_2000803e) {
          DAT_200077a0 = DAT_200077a0 | 0x20000;
        }
        DAT_200077a8 = DAT_200077a8 | 0x20000;
      }
      else {
        LCD_DrawTempUnitIcon();
      }
    }
    else if (g_setup_page == '\x01') {
      DAT_20007798 = DAT_20007798 | 0x20000;
    }
    else if (g_setup_page != '\x02') {
      LCD_DrawTempUnitIcon();
    }
    DAT_2000805b = 1;
  }
  return unaff_r7;
}


// ==== LCD_SubVaAmpsUnit @ 0801a6d8

/* VA current on the sub: COM1 SEG18 (sub A) always, plus COM0 SEG18 (sub m) in modes 13/14/22/23,
   or 15/24 when 0x200080c7 == 0. medium (segment meaning from LCD_DrawSubUnits use) */

void LCD_SubVaAmpsUnit(void)

{
  if ((((g_mode == '\x0e') || (g_mode == '\r')) || (g_mode == '\x17')) || (g_mode == '\x16')) {
    DAT_20007790 = DAT_20007790 | 0x40000;
  }
  else if (((g_mode == '\x0f') || (g_mode == '\x18')) && (DAT_200080c7 == '\0')) {
    DAT_20007790 = DAT_20007790 | 0x40000;
  }
  DAT_20007798 = DAT_20007798 | 0x40000;
  return;
}


// ==== LCD_ClearSubUnits @ 0801a760

/* clears sub unit segments COM0 SEG18/19, COM1..3 SEG17/18/29 and the sub minus; from
   LCD_DrawSubUnits and Mode_Cycle. high */

void LCD_ClearSubUnits(void)

{
  DAT_20007790 = DAT_20007790 & 0xfff3ffff;
  DAT_20007798 = DAT_20007798 & 0xdff9ffff;
  DAT_200077a0 = DAT_200077a0 & 0xdff9ffff;
  DAT_200077a8 = DAT_200077a8 & 0xdff9ffff;
  return;
}


// ==== LCD_ClearShadow @ 0801a81c

/* clears the LCD shadow RAM (Board_LCDInit arg 2). medium */

void LCD_ClearShadow(void)

{
  int iVar1;
  uint uVar2;
  
  do {
    iVar1 = LCD_GetFlagStatus(4);
  } while (iVar1 != 0);
  for (uVar2 = 0; (uVar2 & 0xff) < 0x10; uVar2 = (uVar2 & 0xff) + 1) {
    (&DAT_20007790)[uVar2 & 0xff] = 0;
  }
  DAT_2000805b = 1;
  return;
}


// ==== LCD_DrawVersion @ 0801a854

/* "V-x.xx" boot text. medium */

void LCD_DrawVersion(void)

{
  LCD_ClearShadow();
  Num_SplitDigits(0xcd);
  Num_ToText(0);
  DAT_20007c30 = 0x56;
  DAT_20007c31 = 0x2d;
  LCD_DrawText5(0,&DAT_20007c30);
  LCD_SetMainDP(2);
  DAT_2000805b = 1;
  return;
}


// ==== LCD_AllOn @ 0801a89c

/* every shadow segment on (all 4x44), dirty flag: display test from Key_ShortPress. high */

void LCD_AllOn(void)

{
  DAT_20007790 = 0xffffffff;
  DAT_20007794 = DAT_20007794 | 0xfff;
  DAT_20007798 = 0xffffffff;
  DAT_2000779c = DAT_2000779c | 0xfff;
  DAT_200077a0 = 0xffffffff;
  DAT_200077a4 = DAT_200077a4 | 0xfff;
  DAT_200077a8 = 0xffffffff;
  DAT_200077ac = DAT_200077ac | 0xfff;
  DAT_2000805b = 1;
  return;
}


// ==== LCD_DrawChar @ 0801a950

/* (ch, pos): ASCII -> 7-segment code (font 0x0801affc) into the LCD shadow at digit pos (0..4 sub,
   5..9 main); clears only the 7 glyph bits, so decimal points survive. high [video: draw glyph,
   confirmed] */

void LCD_DrawChar(undefined1 param_1,byte param_2)

{
  uint uVar1;
  
  uVar1 = LCD_FontLookup(param_1);
  if (param_2 == 0) {
    DAT_20007790 = DAT_20007790 & 0xefffffff | (uVar1 & 0xc0) << 0x15;
    DAT_20007798 = DAT_20007798 & 0xe7ffffff | (uVar1 & 0x30) << 0x17;
    DAT_200077a0 = DAT_200077a0 & 0xe7ffffff | (uVar1 & 0xc) << 0x19;
    DAT_200077a8 = DAT_200077a8 & 0xe7ffffff | (uVar1 & 3) << 0x1b;
  }
  else if (param_2 == 2) {
    DAT_20007790 = DAT_20007790 & 0xfeffffff | (uVar1 & 0xc0) << 0x11;
    DAT_20007798 = DAT_20007798 & 0xfe7fffff | (uVar1 & 0x30) << 0x13;
    DAT_200077a0 = DAT_200077a0 & 0xfe7fffff | (uVar1 & 0xc) << 0x15;
    DAT_200077a8 = DAT_200077a8 & 0xfe7fffff | (uVar1 & 3) << 0x17;
  }
  else if (param_2 < 2) {
    DAT_20007790 = DAT_20007790 | (uVar1 & 0x40) << 0x13;
    DAT_20007798 = DAT_20007798 & 0xfdffffff | (uVar1 & 0x10) << 0x15;
    DAT_200077a0 = DAT_200077a0 & 0xfdffffff | (uVar1 & 4) << 0x17;
    DAT_200077a8 = DAT_200077a8 & 0xfdffffff | (uVar1 & 1) << 0x19;
    DAT_20007794 = DAT_20007794 & 0xeff | (uVar1 & 0x80) << 1;
    DAT_2000779c = DAT_2000779c & 0xeff | (uVar1 & 0x20) << 3;
    DAT_200077a4 = DAT_200077a4 & 0xeff | (uVar1 & 8) << 5;
    DAT_200077ac = DAT_200077ac & 0xeff | (uVar1 & 2) << 7;
  }
  else if (param_2 == 4) {
    DAT_20007790 = DAT_20007790 & 0xffefffff | (uVar1 & 0xc0) << 0xd;
    DAT_20007798 = DAT_20007798 & 0xffe7ffff | (uVar1 & 0x30) << 0xf;
    DAT_200077a0 = DAT_200077a0 & 0xffe7ffff | (uVar1 & 0xc) << 0x11;
    DAT_200077a8 = DAT_200077a8 & 0xffe7ffff | (uVar1 & 3) << 0x13;
  }
  else if (param_2 < 4) {
    DAT_20007790 = DAT_20007790 & 0xffbfffff | (uVar1 & 0xc0) << 0xf;
    DAT_20007798 = DAT_20007798 & 0xff9fffff | (uVar1 & 0x30) << 0x11;
    DAT_200077a0 = DAT_200077a0 & 0xff9fffff | (uVar1 & 0xc) << 0x13;
    DAT_200077a8 = DAT_200077a8 & 0xff9fffff | (uVar1 & 3) << 0x15;
  }
  else if (param_2 == 6) {
    DAT_20007794 = DAT_20007794 & 0xfef | uVar1 >> 3 & 0x18;
    DAT_2000779c = DAT_2000779c & 0xfe7 | uVar1 >> 1 & 0x18;
    DAT_200077a4 = DAT_200077a4 & 0xfe7 | (uVar1 & 0xc) << 1;
    DAT_200077ac = DAT_200077ac & 0xfe7 | (uVar1 & 3) << 3;
  }
  else if (param_2 < 6) {
    DAT_20007794 = DAT_20007794 & 0xfbf | uVar1 >> 1 & 0x60;
    DAT_2000779c = DAT_2000779c & 3999 | (uVar1 & 0x30) << 1;
    DAT_200077a4 = DAT_200077a4 & 3999 | (uVar1 & 0xc) << 3;
    DAT_200077ac = DAT_200077ac & 3999 | (uVar1 & 3) << 5;
  }
  else if (param_2 == 8) {
    DAT_20007790 = DAT_20007790 & 0xffff7fff | (uVar1 & 0xc0) << 8;
    DAT_20007798 = DAT_20007798 & 0xffff3fff | (uVar1 & 0x30) << 10;
    DAT_200077a0 = DAT_200077a0 & 0xffff3fff | (uVar1 & 0xc) << 0xc;
    DAT_200077a8 = DAT_200077a8 & 0xffff3fff | (uVar1 & 3) << 0xe;
  }
  else if (param_2 < 8) {
    DAT_20007794 = DAT_20007794 & 0xffb | uVar1 >> 5 & 6;
    DAT_2000779c = DAT_2000779c & 0xff9 | uVar1 >> 3 & 6;
    DAT_200077a4 = DAT_200077a4 & 0xff9 | uVar1 >> 1 & 6;
    DAT_200077ac = DAT_200077ac & 0xff9 | (uVar1 & 3) << 1;
  }
  else if (param_2 == 9) {
    DAT_20007790 = DAT_20007790 & 0xffffdfff | (uVar1 & 0xc0) << 6;
    DAT_20007798 = DAT_20007798 & 0xffffcfff | (uVar1 & 0x30) << 8;
    DAT_200077a0 = DAT_200077a0 & 0xffffcfff | (uVar1 & 0xc) << 10;
    DAT_200077a8 = DAT_200077a8 & 0xffffcfff | (uVar1 & 3) << 0xc;
  }
  return;
}


// ==== LCD_FontLookup @ 0801affc

/* ASCII -> 7-segment code: digits table 0x08029154, A-Z/a-z tables, ' ' 0, '-' 4, '=' 0x82, 0xFF
   0xBF, else 0; used by LCD_DrawChar. high (emulated) */

undefined1 LCD_FontLookup(byte param_1)

{
  undefined1 uVar1;
  
  uVar1 = 0;
  if ((param_1 < 0x30) || (0x39 < param_1)) {
    if ((param_1 < 0x5b) && (0x40 < param_1)) {
      uVar1 = *(undefined1 *)((int)&DAT_08029074 + param_1 + 3);
    }
    else if ((param_1 < 0x7b) && (0x60 < param_1)) {
      uVar1 = *(undefined1 *)((int)&DAT_08029070 + param_1 + 3);
    }
    else if (param_1 == 0x20) {
      uVar1 = 0;
    }
    else if (param_1 == 0x2d) {
      uVar1 = 4;
    }
    else if (param_1 == 0x3d) {
      uVar1 = 0x82;
    }
    else if (param_1 == 0xff) {
      uVar1 = 0xbf;
    }
  }
  else {
    uVar1 = (&DAT_08029154)[param_1];
  }
  return uVar1;
}


// ==== Num_SplitDigits @ 0801b090

/* (v) 5 decimal digits of v -> 0x20007db0/b4/b8/bc/c0 (ten-thousands..units), input to Num_ToText.
   high */

void Num_SplitDigits(uint param_1)

{
  DAT_20007db0 = param_1 / 10000;
  DAT_20007db4 = (param_1 % 10000) / 1000;
  DAT_20007db8 = (param_1 % 1000) / 100;
  DAT_20007dbc = (param_1 % 100) / 10;
  DAT_20007dc0 = param_1 % 10;
  return;
}


// ==== Num_ToText @ 0801b130

/* (dp): digits 0x20007db0..0x20007dc0 (from 0x0801b090) -> ASCII text 0x20007c30, leading zeros
   blanked up to the decimal point (dp 0: 4 digits, 1: 3, 2: 2, 3: 1, 4: none); dead > 0xff guards.
   high */

void Num_ToText(byte param_1)

{
  if (0xff < DAT_20007db0) {
    DAT_20007db0 = 0;
  }
  if (0xff < DAT_20007db4) {
    DAT_20007db4 = 0;
  }
  if (0xff < DAT_20007db8) {
    DAT_20007db8 = 0;
  }
  if (0xff < DAT_20007dbc) {
    DAT_20007dbc = 0;
  }
  if (0xff < DAT_20007dc0) {
    DAT_20007dc0 = 0;
  }
  if (param_1 == 0) {
    if ((((DAT_20007db0 == 0) && (DAT_20007db0 = 0x20, DAT_20007db4 == 0)) &&
        (DAT_20007db4 = 0x20, DAT_20007db8 == 0)) && (DAT_20007db8 = 0x20, DAT_20007dbc == 0)) {
      DAT_20007dbc = 0x20;
    }
  }
  else if (param_1 == 2) {
    if ((DAT_20007db0 == 0) && (DAT_20007db0 = 0x20, DAT_20007db4 == 0)) {
      DAT_20007db4 = 0x20;
    }
  }
  else if (param_1 < 2) {
    if (((DAT_20007db0 == 0) && (DAT_20007db0 = 0x20, DAT_20007db4 == 0)) &&
       (DAT_20007db4 = 0x20, DAT_20007db8 == 0)) {
      DAT_20007db8 = 0x20;
    }
  }
  else if ((param_1 == 3) && (DAT_20007db0 == 0)) {
    DAT_20007db0 = 0x20;
  }
  if (DAT_20007db0 < 10) {
    DAT_20007db0 = DAT_20007db0 + 0x30;
  }
  if (DAT_20007db4 < 10) {
    DAT_20007db4 = DAT_20007db4 + 0x30;
  }
  if (DAT_20007db8 < 10) {
    DAT_20007db8 = DAT_20007db8 + 0x30;
  }
  if (DAT_20007dbc < 10) {
    DAT_20007dbc = DAT_20007dbc + 0x30;
  }
  if (DAT_20007dc0 < 10) {
    DAT_20007dc0 = DAT_20007dc0 + 0x30;
  }
  DAT_20007c30 = (char)DAT_20007db0;
  DAT_20007c31 = (char)DAT_20007db4;
  DAT_20007c32 = (char)DAT_20007db8;
  DAT_20007c33 = (char)DAT_20007dbc;
  DAT_20007c34 = (char)DAT_20007dc0;
  return;
}


// ==== LCD_Flush @ 0801b304  [LCD]

/* copies the LCD shadow (0x20007790, 8 words) to LCD->RAM and requests an update. [video] + LCD
   reach here */

undefined4 LCD_Flush(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  if ((byte)(DAT_20008050 & (DAT_20007fe1 ^ 1) & (DAT_20008053 ^ 1)) == 0) {
    DAT_20008053 = 0;
    do {
      iVar1 = LCD_GetFlagStatus(4);
    } while (iVar1 != 0);
    LCD.RAM_COM0 = DAT_20007790;
    LCD._24_4_ = DAT_20007794;
    LCD.RAM_COM1 = DAT_20007798;
    LCD._32_4_ = DAT_2000779c;
    LCD.RAM_COM2 = DAT_200077a0;
    LCD._40_4_ = DAT_200077a4;
    LCD.RAM_COM3 = DAT_200077a8;
    LCD._48_4_ = DAT_200077ac;
    LCD_UpdateDisplayRequest();
  }
  else {
    LCD_FlushBarOnly();
  }
  return unaff_r7;
}


// ==== LCD_FlushBarOnly @ 0801b396  [LCD]

/* LCD_Flush during auto-hold (not logging, 0x20008053 clear): copies only bargraph bits (COM0
   0x14FE, COM1 0x8FF, COM2 0xFF, COM3 0x1FE) so digits freeze, bar stays live; no UDR wait first.
   high */

undefined4 LCD_FlushBarOnly(void)

{
  dword dVar1;
  undefined4 unaff_r7;
  
  dVar1 = LCD.RAM_COM0;
  LCD.RAM_COM0 = dVar1 & 0xffffeb01;
  dVar1 = LCD.RAM_COM0;
  LCD.RAM_COM0 = dVar1 | DAT_20007790 & 0x14fe;
  dVar1 = LCD.RAM_COM1;
  LCD.RAM_COM1 = dVar1 & 0xfffff700;
  dVar1 = LCD.RAM_COM1;
  LCD.RAM_COM1 = dVar1 | DAT_20007798 & 0x8ff;
  dVar1 = LCD.RAM_COM2;
  LCD.RAM_COM2 = dVar1 & 0xffffff00;
  dVar1 = LCD.RAM_COM2;
  LCD.RAM_COM2 = dVar1 | (byte)DAT_200077a0;
  dVar1 = LCD.RAM_COM3;
  LCD.RAM_COM3 = dVar1 & 0xfffffe01;
  dVar1 = LCD.RAM_COM3;
  LCD.RAM_COM3 = dVar1 | DAT_200077a8 & 0x1fe;
  LCD_UpdateDisplayRequest();
  return unaff_r7;
}


// ==== Board_LCDInit @ 0801b450

/* LCD controller init with the saved contrast (DAT_20000237). [video: PS=3 DIV=0 1/4 duty 1/3 bias
   PON=4] */

void Board_LCDInit(undefined1 param_1,char param_2)

{
  int iVar1;
  undefined4 local_20;
  undefined4 local_1c;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  
  LCD_PinsInit();
  RCC_APB1PeriphClockCmd(0x200,1);
  local_20 = 0xc00000;
  local_1c = 0;
  local_18 = 0xc;
  local_14 = 0x40;
  local_10 = 0;
  LCD_Init(&local_20);
  LCD_MuxSegmentCmd(0);
  LCD_SetContrast(param_1);
  LCD_DeadTimeConfig(0);
  LCD_PulseOnDurationConfig(0x40);
  LCD_WaitForSynchro();
  LCD_Cmd(1);
  do {
    iVar1 = LCD_GetFlagStatus(1);
  } while (iVar1 == 0);
  do {
    iVar1 = LCD_GetFlagStatus(0x10);
  } while (iVar1 == 0);
  LCD_BlinkConfig(0,0x4000);
  if (param_2 != '\0') {
    LCD_ClearShadow();
  }
  LCD_Flush();
  return;
}


// ==== LCD_PinsInit @ 0801b4d6  [GPIOA GPIOB GPIOC GPIOD GPIOE]

/* LCD segment/COM pins on GPIOA..E as alternate function (562 B of GPIO setup). medium (by
   peripherals; body to read) */

undefined8 LCD_PinsInit(void)

{
  undefined4 local_10;
  undefined4 local_c;
  
  Board_GPIOClocksOn();
  GPIO_StructInit(&local_10);
  local_10 = 0x87ce;
  local_c._0_1_ = 2;
  GPIO_Init(&GPIOA,&local_10);
  GPIO_PinAFConfig(&GPIOA,1,0xb);
  GPIO_PinAFConfig(&GPIOA,2,0xb);
  GPIO_PinAFConfig(&GPIOA,3,0xb);
  GPIO_PinAFConfig(&GPIOA,6,0xb);
  GPIO_PinAFConfig(&GPIOA,7,0xb);
  GPIO_PinAFConfig(&GPIOA,8,0xb);
  GPIO_PinAFConfig(&GPIOA,9,0xb);
  GPIO_PinAFConfig(&GPIOA,10,0xb);
  GPIO_PinAFConfig(&GPIOA,0xf,0xb);
  local_10 = 0xff3b;
  local_c._0_1_ = 2;
  GPIO_Init(&GPIOB,&local_10);
  GPIO_PinAFConfig(&GPIOB,0,0xb);
  GPIO_PinAFConfig(&GPIOB,1,0xb);
  GPIO_PinAFConfig(&GPIOB,3,0xb);
  GPIO_PinAFConfig(&GPIOB,4,0xb);
  GPIO_PinAFConfig(&GPIOB,5,0xb);
  GPIO_PinAFConfig(&GPIOB,8,0xb);
  GPIO_PinAFConfig(&GPIOB,9,0xb);
  GPIO_PinAFConfig(&GPIOB,10,0xb);
  GPIO_PinAFConfig(&GPIOB,0xb,0xb);
  GPIO_PinAFConfig(&GPIOB,0xc,0xb);
  GPIO_PinAFConfig(&GPIOB,0xd,0xb);
  GPIO_PinAFConfig(&GPIOB,0xe,0xb);
  GPIO_PinAFConfig(&GPIOB,0xf,0xb);
  local_10 = 0x6ff;
  local_c._0_1_ = 2;
  GPIO_Init(&GPIOC,&local_10);
  GPIO_PinAFConfig(&GPIOC,0,0xb);
  GPIO_PinAFConfig(&GPIOC,1,0xb);
  GPIO_PinAFConfig(&GPIOC,2,0xb);
  GPIO_PinAFConfig(&GPIOC,3,0xb);
  GPIO_PinAFConfig(&GPIOC,4,0xb);
  GPIO_PinAFConfig(&GPIOC,5,0xb);
  GPIO_PinAFConfig(&GPIOC,6,0xb);
  GPIO_PinAFConfig(&GPIOC,7,0xb);
  GPIO_PinAFConfig(&GPIOC,9,0xb);
  GPIO_PinAFConfig(&GPIOC,10,0xb);
  local_10 = 0xff00;
  local_c._0_1_ = 2;
  GPIO_Init(&GPIOD,&local_10);
  GPIO_PinAFConfig(&GPIOD,8,0xb);
  GPIO_PinAFConfig(&GPIOD,9,0xb);
  GPIO_PinAFConfig(&GPIOD,10,0xb);
  GPIO_PinAFConfig(&GPIOD,0xb,0xb);
  GPIO_PinAFConfig(&GPIOD,0xc,0xb);
  GPIO_PinAFConfig(&GPIOD,0xd,0xb);
  GPIO_PinAFConfig(&GPIOD,0xe,0xb);
  GPIO_PinAFConfig(&GPIOD,0xf,0xb);
  local_10 = 0xf;
  local_c = CONCAT31(local_c._1_3_,2);
  GPIO_Init(&GPIOE,&local_10);
  GPIO_PinAFConfig(&GPIOE,0,0xb);
  GPIO_PinAFConfig(&GPIOE,1,0xb);
  GPIO_PinAFConfig(&GPIOE,2,0xb);
  GPIO_PinAFConfig(&GPIOE,3,0xb);
  return CONCAT44(local_c,local_10);
}


// ==== LCD_SetContrast @ 0801b708

/* setting 10..17 -> LCD_ContrastConfig((v-10) << 10). high */

void LCD_SetContrast(byte param_1)

{
  if (param_1 == 10) {
    LCD_ContrastConfig(0);
  }
  else if (9 < param_1) {
    if (param_1 == 0xc) {
      LCD_ContrastConfig(0x800);
    }
    else if (param_1 < 0xc) {
      LCD_ContrastConfig(0x400);
    }
    else if (param_1 == 0xe) {
      LCD_ContrastConfig(0x1000);
    }
    else if (param_1 < 0xe) {
      LCD_ContrastConfig(0xc00);
    }
    else if (param_1 == 0x10) {
      LCD_ContrastConfig(0x1800);
    }
    else if (param_1 < 0x10) {
      LCD_ContrastConfig(0x1400);
    }
    else if (param_1 == 0x11) {
      LCD_ContrastConfig(0x1c00);
    }
  }
  return;
}


// ==== LCD_ApplyContrast @ 0801b77a  [LCD]

/* App_StateInit: g_lcd_contrast 10..17 -> LCD_ContrastConfig(LCD->FCR & level) (ANDs the level with
   current FCR instead of using it; out of range passes stale r4). medium */

void LCD_ApplyContrast(void)

{
  dword dVar1;
  uint unaff_r4;
  
  if (g_lcd_contrast == 10) {
    dVar1 = LCD.FCR;
    unaff_r4 = 0;
  }
  else if (9 < g_lcd_contrast) {
    if (g_lcd_contrast == 0xc) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x800;
    }
    else if (g_lcd_contrast < 0xc) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x400;
    }
    else if (g_lcd_contrast == 0xe) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x1000;
    }
    else if (g_lcd_contrast < 0xe) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0xc00;
    }
    else if (g_lcd_contrast == 0x10) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x1800;
    }
    else if (g_lcd_contrast < 0x10) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x1400;
    }
    else if (g_lcd_contrast == 0x11) {
      dVar1 = LCD.FCR;
      unaff_r4 = dVar1 & 0x1c00;
    }
  }
  LCD_ContrastConfig(unaff_r4);
  return;
}


// ==== __aeabi_ddiv @ 0801b874

/* library (IAR/AEABI double divide): 6.0 / 0.6 -> 10. high (emulated) */

ulonglong __aeabi_ddiv(uint param_1,uint param_2,uint param_3,uint param_4)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  uint unaff_r7;
  uint uVar9;
  uint uVar10;
  int iVar11;
  uint uVar12;
  uint uVar13;
  undefined1 in_CY;
  bool bVar14;
  
  uVar12 = (param_2 ^ param_4) & 0x80000000;
  uVar5 = param_2 >> 0x14 & 0x7ff;
  bVar14 = uVar5 == 0;
  if (!bVar14) {
    unaff_r7 = param_4 >> 0x14 & 0x7ff;
    bVar14 = unaff_r7 == 0;
  }
  if (!bVar14) {
    in_CY = 0x7fe < uVar5;
    bVar14 = uVar5 == 0x7ff;
  }
  if (!bVar14) {
    in_CY = 0x7fe < unaff_r7;
    bVar14 = unaff_r7 == 0x7ff;
  }
  if (bVar14) {
    if (uVar5 != 0x7ff) {
      unaff_r7 = param_4 >> 0x14 & 0x7ff;
    }
    if (uVar5 == 0x7ff || unaff_r7 == 0x7ff) {
      uVar5 = param_2 * 2;
      if (uVar5 < 0xffe00001) {
        uVar5 = param_4 * 2;
      }
      if (uVar5 == 0xffe00000) {
        uVar5 = param_2 << 1;
      }
      if (uVar5 < 0xffe00000) {
        if ((param_4 & 0x7fffffff) != 0x7ff00000) {
          uVar12 = uVar12 | 0x7ff00000;
        }
      }
      else {
        uVar12 = 0xffffffff;
      }
      return (ulonglong)uVar12 << 0x20;
    }
    bVar1 = (param_2 & 0x7fffffff) == 0;
    bVar14 = param_1 == 0 && bVar1;
    uVar8 = 0;
    if (param_1 != 0 || !bVar1) {
      uVar8 = param_3 | param_4 << 1;
      bVar14 = uVar8 == 0;
    }
    if (bVar14) {
      if (param_1 != 0 || (param_2 & 0x7fffffff) != 0) {
        return CONCAT44(param_2 ^ param_4,uVar8) & 0x80000000ffffffff | 0x7ff0000000000000;
      }
      if (param_3 == 0 && (param_4 & 0x7fffffff) == 0) {
        uVar12 = ~uVar8;
      }
      return CONCAT44(uVar12,uVar8);
    }
    if (unaff_r7 == 0) {
      uVar3 = param_3 * 2;
      iVar4 = param_4 * 2 + (uint)CARRY4(param_3,param_3);
      iVar11 = LZCOUNT(iVar4);
      if (iVar4 == 0) {
        iVar11 = LZCOUNT(uVar3) + 0x20;
      }
      uVar8 = iVar11 - 0xb;
      if (uVar8 < 0x20) {
        param_4 = iVar4 << (uVar8 & 0xff) | uVar3 >> (0x20 - uVar8 & 0xff);
      }
      else {
        param_4 = uVar3 << (iVar11 - 0x2bU & 0xff);
      }
      param_3 = uVar3 << (uVar8 & 0xff);
      if (uVar5 != 0) {
        iVar4 = uVar5 + uVar8 + -1;
        goto LAB_0801b896;
      }
    }
    else {
      uVar8 = -unaff_r7;
    }
    param_2 = param_2 & 0x7fffffff;
    iVar4 = LZCOUNT(param_2);
    if (param_2 == 0) {
      iVar4 = LZCOUNT(param_1) + 0x20;
    }
    uVar5 = iVar4 - 0xb;
    if (uVar5 < 0x20) {
      param_2 = param_2 << (uVar5 & 0xff) | param_1 >> (0x20 - uVar5 & 0xff);
    }
    else {
      param_2 = param_1 << (iVar4 - 0x2bU & 0xff);
    }
    param_1 = param_1 << (uVar5 & 0xff);
    iVar4 = uVar8 - uVar5;
  }
  else {
    iVar4 = (uVar5 - unaff_r7) - (uint)!(bool)in_CY;
  }
LAB_0801b896:
  param_2 = param_2 & 0xfffff;
  param_4 = param_4 & 0xfffff;
  uVar5 = param_1 - param_3;
  iVar11 = (param_2 - param_4) - (uint)(param_3 > param_1);
  uVar8 = param_4 | 0x100000;
  if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
    iVar4 = iVar4 + -1;
    uVar3 = uVar5 & 0x80000000;
    uVar2 = uVar5 * 2;
    uVar5 = uVar2 + param_3;
    iVar11 = iVar11 * 2 + (uint)(uVar3 != 0) + uVar8 + (uint)CARRY4(uVar2,param_3);
  }
  uVar2 = iVar11 << 0xb | uVar5 >> 0x15;
  uVar7 = uVar2 / uVar8;
  uVar2 = uVar2 - uVar8 * uVar7;
  uVar9 = (uint)((ulonglong)param_3 * (ulonglong)uVar7);
  uVar6 = (uint)((ulonglong)param_3 * (ulonglong)uVar7 >> 0x20);
  uVar5 = uVar5 * 0x800;
  uVar3 = uVar5 - uVar9;
  iVar11 = (uVar2 - uVar6) - (uint)(uVar9 > uVar5);
  if (uVar2 <= uVar6 && (uint)(uVar9 <= uVar5) <= uVar2 - uVar6) {
    uVar7 = uVar7 - 1;
    bVar14 = CARRY4(uVar3,param_3);
    uVar3 = uVar3 + param_3;
    iVar11 = iVar11 + uVar8 + (uint)bVar14;
  }
  uVar2 = iVar11 << 0xb | uVar3 >> 0x15;
  uVar13 = uVar2 / uVar8;
  uVar2 = uVar2 - uVar8 * uVar13;
  uVar9 = (uint)((ulonglong)param_3 * (ulonglong)uVar13);
  uVar6 = (uint)((ulonglong)param_3 * (ulonglong)uVar13 >> 0x20);
  uVar3 = uVar3 * 0x800;
  uVar5 = uVar3 - uVar9;
  iVar11 = (uVar2 - uVar6) - (uint)(uVar9 > uVar3);
  if (uVar2 <= uVar6 && (uint)(uVar9 <= uVar3) <= uVar2 - uVar6) {
    uVar13 = uVar13 - 1;
    bVar14 = CARRY4(uVar5,param_3);
    uVar5 = uVar5 + param_3;
    iVar11 = iVar11 + uVar8 + (uint)bVar14;
  }
  uVar2 = iVar11 << 10 | uVar5 >> 0x16;
  uVar9 = uVar2 / uVar8;
  uVar2 = uVar2 - uVar8 * uVar9;
  uVar10 = (uint)((ulonglong)param_3 * (ulonglong)uVar9);
  uVar6 = (uint)((ulonglong)param_3 * (ulonglong)uVar9 >> 0x20);
  uVar5 = uVar5 * 0x400;
  uVar3 = uVar5 - uVar10;
  iVar11 = (uVar2 - uVar6) - (uint)(uVar10 > uVar5);
  if (uVar2 <= uVar6 && (uint)(uVar10 <= uVar5) <= uVar2 - uVar6) {
    uVar9 = uVar9 - 1;
    bVar14 = CARRY4(uVar3,param_3);
    uVar3 = uVar3 + param_3;
    iVar11 = iVar11 + uVar8 + (uint)bVar14;
  }
  uVar2 = iVar11 << 0xb | uVar3 >> 0x15;
  uVar9 = uVar7 << 0x15 | uVar13 << 10 | uVar9;
  uVar7 = uVar2 / uVar8;
  uVar2 = uVar2 - uVar8 * uVar7;
  uVar13 = (uint)((ulonglong)param_3 * (ulonglong)uVar7);
  uVar6 = (uint)((ulonglong)param_3 * (ulonglong)uVar7 >> 0x20);
  uVar3 = uVar3 * 0x800;
  uVar5 = uVar3 - uVar13;
  iVar11 = (uVar2 - uVar6) - (uint)(uVar13 > uVar3);
  if (uVar2 <= uVar6 && (uint)(uVar13 <= uVar3) <= uVar2 - uVar6) {
    uVar7 = uVar7 - 1;
    bVar14 = CARRY4(uVar5,param_3);
    uVar5 = uVar5 + param_3;
    iVar11 = iVar11 + uVar8 + (uint)bVar14;
  }
  uVar3 = iVar11 << 0xb | uVar5 >> 0x15;
  uVar6 = uVar3 / uVar8;
  uVar3 = uVar3 - uVar8 * uVar6;
  uVar8 = (uint)((ulonglong)param_3 * (ulonglong)uVar6);
  uVar2 = (uint)((ulonglong)param_3 * (ulonglong)uVar6 >> 0x20);
  uVar5 = uVar5 * 0x800;
  uVar6 = uVar6 - (uVar3 <= uVar2 && (uint)(uVar8 <= uVar5) <= uVar3 - uVar2);
  if (uVar3 - uVar2 != (uint)(uVar8 > uVar5) || uVar5 != uVar8) {
    uVar6 = uVar6 | 1;
  }
  uVar5 = (uVar6 & 0xffc007ff | (uVar7 & 0x7ff) << 0xb) >> 2;
  iVar11 = iVar4 + 0x400;
  if (iVar11 != 0 && -0x401 < iVar4) {
    bVar14 = 0x80000000 < uVar6 * 0x40000000 ||
             uVar6 * 0x40000000 + 0x80000000 < (uint)((uVar6 & 4) != 0);
    uVar8 = uVar9 * 0x100000;
    uVar3 = (uVar9 >> 0xc) +
            iVar11 * 0x100000 + (uint)(CARRY4(uVar5,uVar8) || CARRY4(uVar5 + uVar8,(uint)bVar14));
    if ((int)(uVar3 + 0x100000) < 0) {
      return (ulonglong)(uVar12 | 0x7ff00000) << 0x20;
    }
    return CONCAT44(uVar3 | uVar12,uVar5 + uVar8 + bVar14);
  }
  uVar2 = (uVar6 & 3) << 0x1d;
  uVar5 = uVar5 | uVar9 << 0x14;
  uVar8 = uVar9 >> 0xc | 0x100000;
  uVar3 = -iVar11 + 1;
  uVar6 = -iVar11 - 0x1f;
  if ((int)uVar3 < 0x20) {
    uVar13 = uVar3 & 0xff;
    uVar10 = uVar3 & 0xff;
    uVar6 = uVar8 >> (uVar3 & 0xff);
    uVar7 = uVar5 >> (uVar3 & 0xff) | uVar8 << (0x20 - uVar3 & 0xff);
    uVar9 = 0x20 - uVar3 & 0xff;
    bVar14 = uVar9 == 0 &&
             (uVar10 == 0 &&
              (uVar13 == 0 && 0x1f < uVar3 || uVar13 != 0 && (uVar5 >> uVar13 - 1 & 1) != 0) ||
             uVar10 != 0 && (uVar8 >> uVar10 - 1 & 1) != 0) ||
             uVar9 != 0 && (uVar5 << uVar9 - 1 & 0x80000000) != 0;
    uVar2 = uVar2 | uVar5 << uVar9;
  }
  else {
    if (0x34 < (int)uVar3) {
      return (ulonglong)uVar12 << 0x20;
    }
    uVar7 = uVar8 >> (uVar6 & 0xff);
    uVar3 = 0x20 - uVar6 & 0xff;
    bVar14 = uVar3 != 0 && (uVar8 << uVar3 - 1 & 0x80000000) != 0;
    uVar2 = (uVar2 | uVar5 << 1) >> 1 | uVar5 >> (uVar6 & 0xff) | uVar8 << uVar3;
    uVar6 = 0;
  }
  bVar14 = 0x80000000 < uVar2 || uVar2 + 0x80000000 < (uint)bVar14;
  return CONCAT44(uVar6 + uVar12 + CARRY4(uVar7,(uint)bVar14),uVar7 + bVar14);
}


// ==== __aeabi_d2f @ 0801bacc

/* library: double (r0:r1) -> float, round to nearest, overflow to inf. high (emulated) */

ulonglong __aeabi_d2f(uint param_1,uint param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  
  uVar2 = param_2 >> 0x1f;
  uVar3 = uVar2 | param_2 << 1;
  uVar5 = uVar3 + 0x90000000;
  if (uVar3 < 0x70000000 || uVar5 < 0x200000) {
    uVar3 = param_2 << 0xb | param_1 >> 0x15;
    if ((param_1 & 0x1fffff) != 0) {
      uVar3 = uVar3 | 1;
    }
    uVar4 = uVar3 | 0x80000000;
    uVar5 = 9 - ((int)uVar5 >> 0x15);
    if (uVar5 < 0x21) {
      uVar5 = 0x20 - (uVar5 | uVar2 << 0x1f);
      uVar2 = uVar5 & 0xff;
      uVar5 = 0x20 - uVar5;
      return CONCAT44(uVar3,(uVar4 >> (uVar5 & 0xff)) +
                            (uVar5 & 0x80000000) +
                            (uint)(0x80000000 < uVar4 << uVar2 ||
                                  (uVar4 << uVar2) + 0x80000000 <
                                  (uint)(uVar2 != 0 && (uVar4 << uVar2 - 1 & 0x80000000) != 0))) |
             0x8000000000000000;
    }
    return CONCAT44(uVar3,uVar2 << 0x1f) | 0x8000000000000000;
  }
  if ((uVar5 & 0x80000000) == 0 && uVar5 * 2 < 0x3fc00000) {
    bVar1 = (param_1 & 0x20000000) != 0;
    uVar2 = param_1 * 8;
    return CONCAT44((uVar2 + 0x80000000) - (uint)!bVar1,
                    ((uint)((uVar5 & 1) != 0) << 0x1f | ((uVar5 & 0x7fffffff) >> 1 & 0xfffffff) << 3
                    ) + (param_1 >> 0x1d) +
                        (uint)(0x80000000 < uVar2 || uVar2 + 0x80000000 < (uint)bVar1));
  }
  param_2 = param_2 * 2;
  if (param_2 < 0xffe00001) {
    return CONCAT44(param_2,(uVar3 * 0x100 + 0xff) * 0x800000);
  }
  return CONCAT44(param_2,(int)param_2 >> 0x1f);
}


// ==== log10 @ 0801bb5c  [TIM2]

/* library: r2 = 1, tail-calls the Dinkumware-style log core 0x080204c4 (decimal flag); 1000 -> 3, 2
   -> 0.30103. high (emulated) */

int log10(int param_1,uint param_2)

{
  uint uVar1;
  int iVar2;
  undefined4 *puVar3;
  int iVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  undefined4 uVar7;
  undefined4 uVar8;
  undefined4 uVar9;
  undefined4 uVar10;
  short sVar11;
  char cVar12;
  undefined8 uVar13;
  undefined8 uVar14;
  undefined8 uVar15;
  undefined8 uVar16;
  undefined8 local_30;
  uint uStack_28;
  int local_24;
  
  local_24 = 1;
  uVar1 = (param_2 & 0x7fffffff) >> 0x14;
  local_30._0_4_ = param_1;
  local_30._4_4_ = param_2;
  uStack_28 = param_2;
  if (uVar1 == 0x7ff) {
    sVar11 = 0;
    iVar2 = param_2 << 0xc;
    iVar4 = iVar2;
    if (iVar2 == 0) {
      iVar4 = param_1;
    }
    if ((iVar2 != 0 || iVar4 != 0) || (-1 < (int)param_2)) {
      return param_1;
    }
  }
  else {
    if ((uVar1 == 0) && (uVar1 = _Dnorm(&local_30), 0 < (int)uVar1)) {
      puVar3 = (undefined4 *)__aeabi_errno_addr();
      *puVar3 = 0x22;
      return 0;
    }
    local_30._4_4_ = local_30._4_4_ & 0x800fffff | 0x3fe00000;
    sVar11 = (short)uVar1 + -0x3fe;
  }
  cVar12 = '\x01';
  if ((int)local_30._4_4_ < 0) {
    puVar3 = (undefined4 *)__aeabi_errno_addr();
    *puVar3 = 0x21;
    iVar4 = -1;
  }
  else {
    __aeabi_cdcmple((int)local_30,local_30._4_4_,0x667f3bcd,0x3fe6a09e);
    uVar13 = CONCAT44(local_30._4_4_,(int)local_30);
    if (cVar12 == '\0') {
      uVar13 = __aeabi_dmul(0,0x40000000,(int)local_30,local_30._4_4_);
      sVar11 = sVar11 + -1;
    }
    local_30._4_4_ = (uint)((ulonglong)uVar13 >> 0x20);
    local_30._0_4_ = (int)uVar13;
    uVar14 = __aeabi_dadd(0,0xbff00000,(int)local_30,local_30._4_4_);
    local_30 = uVar13;
    uVar8 = (undefined4)((ulonglong)uVar14 >> 0x20);
    uVar5 = (undefined4)uVar14;
    uVar13 = __aeabi_dadd(0,0x3ff00000,(int)local_30,local_30._4_4_);
    uVar13 = __aeabi_ddiv(uVar5,uVar8,(int)uVar13,(int)((ulonglong)uVar13 >> 0x20));
    uVar9 = (undefined4)((ulonglong)uVar13 >> 0x20);
    uVar6 = (undefined4)uVar13;
    uVar13 = __aeabi_dmul(uVar6,uVar9,uVar6,uVar9);
    uVar10 = (undefined4)((ulonglong)uVar13 >> 0x20);
    uVar7 = (undefined4)uVar13;
    uVar13 = __aeabi_dmul(0xbf29def7,0x3fc2f8d8,uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0xccfeb3f4,0x3fc3991a);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0xd968cb62,0x3fc7466f);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0xda12c9c6,0x3fcc71c4);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0x948ae94a,0x3fd24924);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0x99975ff7,0x3fd99999);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),0x555555bc,0x3fe55555);
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar7,uVar10);
    uVar13 = __aeabi_dsub(uVar5,uVar8,(int)uVar13,(int)((ulonglong)uVar13 >> 0x20));
    uVar13 = __aeabi_dmul((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar6,uVar9);
    uVar9 = (undefined4)((ulonglong)uVar13 >> 0x20);
    uVar6 = (undefined4)uVar13;
    uVar13 = __aeabi_i2d((int)sVar11);
    uVar10 = (undefined4)((ulonglong)uVar13 >> 0x20);
    uVar7 = (undefined4)uVar13;
    if (local_24 == 0) {
      uVar13 = __aeabi_dmul(0xf79abc9e,0x3e7f7d1c,uVar7,uVar10);
      uVar13 = __aeabi_dsub((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar6,uVar9);
      uVar13 = __aeabi_dadd((int)uVar13,(int)((ulonglong)uVar13 >> 0x20),uVar5,uVar8);
      uVar14 = __aeabi_dmul(0xc0000000,0x3fe62e42,uVar7,uVar10);
    }
    else if (local_24 < 1) {
      uVar14 = __aeabi_dmul(0x60000000,0x3ff71547,uVar5,uVar8);
      uVar15 = __aeabi_dmul(0xf85ddf44,0x3e54ae0b,uVar5,uVar8);
      uVar16 = __aeabi_dmul(0x652b82fe,0x3ff71547,uVar6,uVar9);
      uVar15 = __aeabi_dsub((int)uVar15,(int)((ulonglong)uVar15 >> 0x20),(int)uVar16,
                            (int)((ulonglong)uVar16 >> 0x20));
      uVar14 = __aeabi_dadd((int)uVar15,(int)((ulonglong)uVar15 >> 0x20),(int)uVar14,
                            (int)((ulonglong)uVar14 >> 0x20));
    }
    else {
      uVar13 = __aeabi_dmul(0,0x3fdbcb7b,uVar5,uVar8);
      uVar14 = __aeabi_dmul(0xe32a6ab,0x3e5526e5,uVar5,uVar8);
      uVar15 = __aeabi_dmul(0x7fbcc47c,0x3e7427de,uVar7,uVar10);
      uVar16 = __aeabi_dmul(uVar6,uVar9,0x1526e50e,0x3fdbcb7b);
      uVar15 = __aeabi_dsub((int)uVar15,(int)((ulonglong)uVar15 >> 0x20),(int)uVar16,
                            (int)((ulonglong)uVar16 >> 0x20));
      uVar14 = __aeabi_dadd((int)uVar15,(int)((ulonglong)uVar15 >> 0x20),(int)uVar14,
                            (int)((ulonglong)uVar14 >> 0x20));
      uVar13 = __aeabi_dadd((int)uVar14,(int)((ulonglong)uVar14 >> 0x20),(int)uVar13,
                            (int)((ulonglong)uVar13 >> 0x20));
      uVar14 = __aeabi_dmul(0,0x3fd34413,uVar7,uVar10);
    }
    iVar4 = __aeabi_dadd((int)uVar14,(int)((ulonglong)uVar14 >> 0x20),(int)uVar13,
                         (int)((ulonglong)uVar13 >> 0x20));
  }
  return iVar4;
}


// ==== __aeabi_dmul @ 0801bb64

/* library (IAR/AEABI double multiply): 3.0 * 2.5 -> 7.5. high (emulated) */

longlong __aeabi_dmul(uint param_1,uint param_2,uint param_3,uint param_4)

{
  ulonglong uVar1;
  longlong lVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  int iVar8;
  uint unaff_r7;
  uint uVar9;
  int iVar10;
  uint uVar11;
  bool bVar12;
  bool bVar13;
  bool bVar14;
  
  uVar11 = (param_2 ^ param_4) & 0x80000000;
  uVar7 = param_2 >> 0x14 & 0x7ff;
  bVar12 = uVar7 == 0;
  if (!bVar12) {
    unaff_r7 = param_4 >> 0x14 & 0x7ff;
    bVar12 = unaff_r7 == 0;
  }
  if (!bVar12) {
    bVar12 = uVar7 == 0x7ff;
  }
  if (!bVar12) {
    bVar12 = unaff_r7 == 0x7ff;
  }
  if (bVar12) {
    uVar9 = param_4 >> 0x14 & 0x7ff;
    if (uVar7 == 0x7ff || uVar9 == 0x7ff) {
      bVar12 = (param_2 & 0x7fffffff) == 0;
      bVar13 = param_1 == 0 && bVar12;
      if (param_1 != 0 || !bVar12) {
        bVar13 = param_3 == 0 && (param_4 & 0x7fffffff) == 0;
      }
      if (bVar13) {
        param_2 = 0xffffffff;
      }
      uVar7 = param_4 * 2;
      if (uVar7 < 0xffe00001) {
        uVar7 = param_2 * 2;
        param_4 = param_2;
      }
      if (uVar7 < 0xffe00001) {
        return (ulonglong)(uVar11 | 0x7ff00000) << 0x20;
      }
      goto LAB_0801bc12;
    }
    bVar13 = (param_2 & 0x7fffffff) == 0;
    bVar12 = param_1 == 0 && bVar13;
    if (param_1 != 0 || !bVar13) {
      bVar12 = param_3 == 0 && (param_4 & 0x7fffffff) == 0;
    }
    if (!bVar12) {
      if (uVar7 != 0) {
        param_4 = param_4 & 0x7fffffff;
        iVar8 = LZCOUNT(param_4);
        if (param_4 == 0) {
          iVar8 = LZCOUNT(param_3) + 0x20;
        }
        uVar4 = iVar8 - 0xb;
        if (uVar4 < 0x20) {
          uVar6 = param_4 << (uVar4 & 0xff) | param_3 >> (0x20 - uVar4 & 0xff);
        }
        else {
          uVar6 = param_3 << (iVar8 - 0x2bU & 0xff);
        }
        param_3 = param_3 << (uVar4 & 0xff);
        iVar8 = ((uVar7 + uVar9) - uVar4) + 1;
        goto LAB_0801bb8c;
      }
      if (uVar9 != 0) {
        uVar7 = param_1;
        uVar6 = param_2 << 0xc;
        if (param_2 << 0xc == 0) {
          uVar7 = 0;
          uVar9 = uVar9 - 0x14;
          uVar6 = param_1;
        }
        iVar10 = LZCOUNT(uVar6);
        iVar8 = uVar9 - iVar10;
        param_1 = uVar7 << iVar10 + 0xc;
        uVar7 = uVar6 << iVar10 | uVar7 >> (0x20U - (iVar10 + 0xc) & 0xff);
        uVar6 = param_4 & 0x1fffff | 0x100000;
        goto LAB_0801bb98;
      }
    }
  }
  else {
    iVar8 = uVar7 + unaff_r7;
    uVar6 = param_4 & 0x1fffff | 0x100000;
LAB_0801bb8c:
    uVar7 = param_2 << 0xb | 0x80000000 | param_1 >> 0x15;
    param_1 = param_1 << 0xb;
LAB_0801bb98:
    uVar1 = (ulonglong)uVar7 * (ulonglong)param_3 +
            ((ulonglong)param_1 * (ulonglong)param_3 >> 0x20);
    uVar9 = (uint)(uVar1 >> 0x20);
    lVar2 = (ulonglong)uVar6 * (ulonglong)param_1 + (uVar1 & 0xffffffff);
    uVar5 = (uint)lVar2;
    uVar4 = (uint)((ulonglong)lVar2 >> 0x20);
    if ((int)((ulonglong)param_1 * (ulonglong)param_3) != 0) {
      uVar5 = uVar5 | 1;
    }
    uVar1 = (ulonglong)uVar6 * (ulonglong)uVar7 +
            (ulonglong)CONCAT14(CARRY4(uVar9,uVar4),uVar9 + uVar4);
    uVar7 = (uint)uVar1;
    uVar6 = (uint)(uVar1 >> 0x20);
    bVar12 = (uVar1 & 0x10000000000000) != 0;
    if (!bVar12) {
      bVar13 = (uVar5 & 0x80000000) != 0;
      uVar5 = uVar5 << 1;
      bVar14 = CARRY4(uVar7,uVar7) || CARRY4(uVar7 * 2,(uint)bVar13);
      bVar12 = CARRY4(uVar6,uVar6) || CARRY4(uVar6 * 2,(uint)bVar14);
      uVar1 = CONCAT44(uVar6 * 2 + (uint)bVar14,uVar7 * 2 + (uint)bVar13);
    }
    uVar6 = (uint)(uVar1 >> 0x20);
    uVar7 = (uint)uVar1;
    iVar10 = iVar8 + -0x3ff + (uint)bVar12;
    if (iVar10 != 0 &&
        iVar10 < 0 == (SCARRY4(iVar8 + -0x400,1) != SCARRY4(iVar8 + -0x3ff,(uint)bVar12))) {
      bVar12 = 0x80000000 < uVar5 || uVar5 + 0x80000000 < (uint)((uVar1 & 1) != 0);
      iVar8 = uVar6 + iVar10 * 0x100000 + (uint)CARRY4(uVar7,(uint)bVar12);
      if (iVar8 < 0) {
        return (ulonglong)(uVar11 | 0x7ff00000) << 0x20;
      }
      return CONCAT44(iVar8 - 0x100000U | uVar11,uVar7 + bVar12);
    }
    uVar9 = -iVar10 + 1;
    uVar4 = -iVar10 - 0x1f;
    if ((int)uVar9 < 0x20) {
      bVar12 = (uVar5 & 1) != 0;
      uVar5 = uVar5 >> 1;
      if (bVar12) {
        uVar5 = uVar5 | 1;
      }
      uVar4 = 0x20 - uVar9 & 0xff;
      bVar12 = uVar4 == 0 && bVar12 || uVar4 != 0 && (uVar7 << uVar4 - 1 & 0x80000000) != 0;
      uVar5 = uVar5 | uVar7 << uVar4;
      uVar3 = uVar7 >> (uVar9 & 0xff) | uVar6 << (0x20 - uVar9 & 0xff);
      uVar6 = uVar6 >> (uVar9 & 0xff);
LAB_0801bcf8:
      bVar12 = 0x80000000 < uVar5 || uVar5 + 0x80000000 < (uint)bVar12;
      return CONCAT44(uVar6 + uVar11 + CARRY4(uVar3,(uint)bVar12),uVar3 + bVar12);
    }
    if ((int)uVar9 < 0x36) {
      uVar7 = uVar7 >> (uVar4 & 0xff);
      if (uVar5 != 0 || (uVar1 & 0x7fffffff) != 0) {
        uVar7 = uVar7 | 1;
      }
      uVar5 = uVar4 & 0xff;
      uVar3 = uVar6 >> uVar5;
      uVar9 = 0x20 - uVar4 & 0xff;
      bVar12 = uVar9 == 0 &&
               (uVar5 == 0 && (bool)((byte)(uVar1 >> 0x18) >> 7) ||
               uVar5 != 0 && (uVar6 >> uVar5 - 1 & 1) != 0) ||
               uVar9 != 0 && (uVar6 << uVar9 - 1 & 0x80000000) != 0;
      uVar5 = uVar7 | uVar6 << uVar9;
      uVar6 = 0;
      goto LAB_0801bcf8;
    }
  }
  param_1 = 0;
  param_4 = uVar11;
LAB_0801bc12:
  return CONCAT44(param_4,param_1);
}


// ==== __aeabi_cfcmpeq @ 0801bd08

/* library: float compare for equality, result in flags (+0 == -0, NaN unequal); Cal_DivideGain gain
   != 0 test, Mode_Cap_OnCounter. library */

void __aeabi_cfcmpeq(void)

{
  return;
}


// ==== __aeabi_f2uiz @ 0801bd24

/* library: float -> unsigned int, truncating; negatives -> 0, overflow -> 0xFFFFFFFF;
   Mode_Cap_OnCounter, LCD_SubDbm. library */

uint __aeabi_f2uiz(uint param_1)

{
  int iVar1;
  uint uVar2;
  
  if (((param_1 & 0x80000000) != 0) || (iVar1 = ((param_1 & 0x7fffffff) >> 0x17) - 0x7f, iVar1 < 0))
  {
    return 0;
  }
  uVar2 = 0x1f - iVar1;
  if ((int)uVar2 < 0) {
    return 0xffffffff;
  }
  return (param_1 << 8 | 0x80000000) >> (uVar2 & 0xff);
}


// ==== GPIO_Init @ 0801bd48

/* StdPeriph GPIO_Init(port, {pin u32, mode u8, speed u8, otype u8, pupd u8}) */

void GPIO_Init(uint *param_1,uint *param_2)

{
  uint uVar1;
  uint uVar2;
  
  for (uVar1 = 0; uVar1 < 0x10; uVar1 = uVar1 + 1) {
    uVar2 = 1 << (uVar1 & 0xff);
    if ((*param_2 & uVar2) == uVar2) {
      *param_1 = *param_1 & ~(3 << ((uVar1 & 0x7f) << 1)) |
                 (uint)(byte)param_2[1] << ((uVar1 & 0x7f) << 1);
      if (((char)param_2[1] == '\x01') || ((char)param_2[1] == '\x02')) {
        param_1[2] = param_1[2] & ~(3 << ((uVar1 & 0x7f) << 1)) |
                     (uint)*(byte *)((int)param_2 + 5) << ((uVar1 & 0x7f) << 1);
        *(ushort *)(param_1 + 1) =
             (ushort)param_1[1] & ~(ushort)(1 << (uVar1 & 0xff)) |
             (ushort)*(byte *)((int)param_2 + 6) << (uVar1 & 0xff);
      }
      param_1[3] = param_1[3] & ~(3 << ((uVar1 & 0x7f) << 1)) |
                   (uint)*(byte *)((int)param_2 + 7) << ((uVar1 & 0x7f) << 1);
    }
  }
  return;
}


// ==== GPIO_StructInit @ 0801bdec

/* library (StdPeriph): pins 0xFFFF, mode in, speed 400 kHz, push-pull, no pull; LCD_PinsInit.
   library */

void GPIO_StructInit(undefined4 *param_1)

{
  *param_1 = 0xffff;
  *(undefined1 *)(param_1 + 1) = 0;
  *(undefined1 *)((int)param_1 + 5) = 0;
  *(undefined1 *)((int)param_1 + 6) = 0;
  *(undefined1 *)((int)param_1 + 7) = 0;
  return;
}


// ==== GPIO_PinAFConfig @ 0801be04

/* library (StdPeriph): AFR[pin>>3] nibble = af; LCD_PinsInit uses AF11 (LCD). high */

void GPIO_PinAFConfig(int param_1,uint param_2,byte param_3)

{
  *(uint *)(param_1 + ((int)(param_2 & 0xffff) >> 3) * 4 + 0x20) =
       *(uint *)(param_1 + ((int)(param_2 & 0xffff) >> 3) * 4 + 0x20) &
       ~(0xf << ((param_2 & 7) << 2));
  *(uint *)(param_1 + ((int)(param_2 & 0xffff) >> 3) * 4 + 0x20) =
       *(uint *)(param_1 + ((int)(param_2 & 0xffff) >> 3) * 4 + 0x20) |
       (uint)param_3 << ((param_2 & 7) << 2);
  return;
}


// ==== NVIC_EnableIRQ @ 0801be60  [NVIC_ISER0]

/* medium */

void NVIC_EnableIRQ(byte param_1)

{
  (&NVIC_ISER0)[(uint)(int)(char)param_1 >> 5] = 1 << (param_1 & 0x1f);
  return;
}


// ==== Board_Init @ 0801be76

/* standby flag; BOR off via option bytes (resets once); VOS range 1; clocks -> MSI 4.194 MHz;
   RTC/LCD on LSI; debug in low power; GPIO, ADC, timers, NVIC. high */

undefined4 Board_Init(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = PWR_GetFlagStatus(1);
  if (iVar1 != 0) {
    RCC_APB1PeriphClockCmd(0x10000000,1);
    PWR_ClearFlag(2);
  }
  g_woke_from_standby = iVar1 != 0;
  FLASH_ClearFlag(0x1f02);
  g_bor_level_at_boot = FLASH_OB_GetBOR();
  if ((g_bor_level_at_boot & 0xf) != 0) {
    FLASH_OB_Unlock();
    FLASH_ClearFlag(0xf02);
    FLASH_OB_BORConfig(0);
    FLASH_OB_Launch();
  }
  PWR_VoltageScalingConfig(0x800);
  do {
    iVar1 = PWR_GetFlagStatus(0x10);
  } while (iVar1 != 0);
  Board_ClockSelect(0);
  Board_RTCClockInit();
  DBGMCU_Config(7,1);
  Board_GPIOInit();
  Board_AnalogPins();
  Board_ADCInit();
  Board_NVICInit();
  Board_TIM2Init();
  Board_TIM3Init();
  Board_GPIOClocksOn();
  return unaff_r7;
}


// ==== Board_ClockSelect @ 0801bf1a

/* arg 0 (main): HSI on, SYSCLK = MSI range 6 (4.194 MHz), LSE off, HSE off (hangs if HSE stays
   ready); then GPIO/PWR/LCD/SYSCFG/ADC clocks. arg 1: HSE as SYSCLK. high */

undefined4 Board_ClockSelect(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  int iVar2;
  
  if (param_1 == '\0') {
    RCC_HSICmd(1);
    do {
      iVar2 = RCC_GetFlagStatus(0x21);
    } while (iVar2 == 0);
    RCC_SYSCLKConfig(0);
    RCC_MSIRangeConfig(0xc000);
    RCC_LSEConfig(0);
    RCC_HSEConfig(0);
    iVar2 = RCC_GetFlagStatus(0x31);
    if (iVar2 != 0) {
      do {
                    /* WARNING: Do nothing block with infinite loop */
      } while( true );
    }
  }
  else {
    RCC_HSEConfig(1);
    cVar1 = RCC_WaitForHSEStartUp();
    if (cVar1 == '\x01') {
      RCC_SYSCLKConfig(2);
      RCC_HSICmd(0);
    }
  }
  Board_GPIOClocksOn();
  RCC_APB1PeriphClockCmd(0x10000200,1);
  RCC_APB2PeriphClockCmd(0x201,1);
  return param_4;
}


// ==== Board_RTCClockInit @ 0801bf92

/* RTC domain reset, LSI on, RTC/LCD clock = LSI. high */

undefined4 Board_RTCClockInit(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  PWR_RTCAccessCmd(1);
  RCC_RTCResetCmd(1);
  RCC_RTCResetCmd(0);
  RCC_LSICmd(1);
  do {
    iVar1 = RCC_GetFlagStatus(0x41);
  } while (iVar1 == 0);
  RCC_RTCCLKCmd(1);
  RCC_RTCCLKConfig(0x20000);
  return unaff_r7;
}


// ==== Board_GPIOClocksOn @ 0801bfc6

/* RCC_AHBPeriphClockCmd(0xFF): GPIOA..H. high */

undefined4 Board_GPIOClocksOn(void)

{
  undefined4 unaff_r7;
  
  RCC_AHBPeriphClockCmd(0xff,1);
  return unaff_r7;
}


// ==== RCC_AHBPeriphClocksOff @ 0801bfd2

/* RCC_AHBPeriphClockCmd(0xFF, DISABLE): GPIO clocks off; Pins_AllAnalog. high */

undefined4 RCC_AHBPeriphClocksOff(void)

{
  undefined4 unaff_r7;
  
  RCC_AHBPeriphClockCmd(0xff,0);
  return unaff_r7;
}


// ==== Board_NVICInit @ 0801bfde

/* priority group 2 (0x500), four NVIC_Init calls. high */

undefined4 Board_NVICInit(void)

{
  undefined1 *puVar1;
  undefined1 auStack_88 [128];
  
  puVar1 = auStack_88 + 0x80;
  NVIC_PriorityGroupConfig(0x500);
  NVIC_Init(puVar1);
  NVIC_Init(puVar1);
  NVIC_Init(puVar1);
  NVIC_Init(puVar1);
  return 0x101011d;
}


// ==== Board_GPIOInit @ 0801c056  [GPIOA GPIOB GPIOC GPIOD GPIOE GPIOF GPIOH]

/* pin power-on state; ORDER: rotary/keys in, jack sense in, PD1 PWR_CTL2 high, then PE9 PWR_CTL
   high (3.6 V analog VDD on; +4.0 V first via PD1), PE8 backlight + PE7 buzzer low, HY3131 lines
   (CK low, CS/DI high, DO in pull-up), RTC lines, front end via Frontend_SetSwitches(100). high */

undefined8 Board_GPIOInit(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 local_a;
  undefined1 local_9;
  
  Board_GPIOClocksOn();
  Board_RotaryKeysInput(0);
  Board_JackSenseInput();
  local_10 = 0x83;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOD,&local_10);
  GPIOD.BSRR._0_2_ = 2;
  GPIOD.BSRR._0_2_ = 0x80;
  local_10 = 0xb3a0;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOE,&local_10);
  GPIOE.BSRR._0_2_ = 0x200;
  GPIOE.BSRR._2_2_ = 0x100;
  GPIOE.BSRR._2_2_ = 0x80;
  GPIOE.BSRR._2_2_ = 0x1000;
  GPIOE.BSRR._2_2_ = 0x2000;
  GPIOE.BSRR._2_2_ = 0x20;
  local_10 = 4;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOF,&local_10);
  local_10 = 1;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOF,&local_10);
  local_10 = 2;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOF,&local_10);
  local_10 = 8;
  local_c = 0;
  local_9 = 1;
  local_b = 3;
  GPIO_Init(&GPIOF,&local_10);
  GPIOF.BSRR._0_2_ = 2;
  GPIOF.BSRR._0_2_ = 4;
  GPIOF.BSRR._2_2_ = 1;
  local_10 = 0xc0;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOB,&local_10);
  GPIOB.BSRR._2_2_ = 0x40;
  GPIOB.BSRR._2_2_ = 0x80;
  local_10 = 0x2800;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOC,&local_10);
  GPIOC.BSRR._2_2_ = 0x2000;
  local_10 = 0x8070;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOF,&local_10);
  GPIOF.BSRR._2_2_ = 0x8000;
  local_10 = 4;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOH,&local_10);
  local_10 = 0x1800;
  local_c = 1;
  local_a = 0;
  local_9 = 0;
  local_b = 1;
  GPIO_Init(&GPIOA,&local_10);
  GPIOA.BSRR._0_2_ = 0x1000;
  GPIOE.BSRR._2_2_ = 0x8000;
  GPIOF.BSRR._2_2_ = 0x40;
  GPIOA.BSRR._2_2_ = 0x800;
  GPIOH.BSRR._2_2_ = 4;
  Frontend_SetSwitches(100);
  return CONCAT17(local_9,CONCAT16(local_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== Board_RotaryKeysInput @ 0801c2c6  [GPIOG]

/* PG0..7 rotary + PG8..15 keys: inputs, pull-up (arg leaves one rotary line out). high */

undefined8
Board_RotaryKeysInput(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 uStack_a;
  undefined1 local_9;
  
  uStack_a = (undefined1)((uint)param_4 >> 0x10);
  if (param_1 == '\0') {
    local_10 = 0xff;
  }
  else if (param_1 == '\x01') {
    local_10 = 0x7f;
  }
  else if (param_1 == '\x02') {
    local_10 = 0xbf;
  }
  else if (param_1 == '\x04') {
    local_10 = 0xdf;
  }
  else if (param_1 == '\b') {
    local_10 = 0xef;
  }
  else if (param_1 == '\x10') {
    local_10 = 0xf7;
  }
  else if (param_1 == ' ') {
    local_10 = 0xfb;
  }
  else if (param_1 == '@') {
    local_10 = 0xfd;
  }
  else {
    local_10 = param_3;
    if (param_1 == -0x80) {
      local_10 = 0xfe;
    }
  }
  local_c = 0;
  local_9 = 1;
  local_b = 3;
  GPIO_Init(&GPIOG,&local_10);
  local_10 = 0xff00;
  local_c = 0;
  local_9 = 1;
  local_b = 3;
  GPIO_Init(&GPIOG,&local_10);
  return CONCAT17(local_9,CONCAT16(uStack_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== Board_JackSenseInput @ 0801c368  [GPIOE]

/* PE10 (mA) / PE11 (A) jack sense: inputs, no pull. high */

undefined8 Board_JackSenseInput(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 uStack_a;
  undefined1 local_9;
  
  local_10 = 0xc00;
  local_c = 0;
  local_9 = 0;
  local_b = 3;
  GPIO_Init(&GPIOE,&local_10);
  return CONCAT17(local_9,CONCAT16(uStack_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== BLE_Start @ 0801c390  [GPIOD GPIOF USART2]

/* PF15 high (BLE112 power), PD5/PD6 AF7, USART2 19200 8N1 TX+RX (baud from RCC_GetClocksFreq at run
   time), RXNE IRQ, NVIC IRQ 38 on. high */

void BLE_Start(void)

{
  undefined4 local_20;
  undefined1 local_1c;
  undefined1 local_1b;
  undefined1 local_1a;
  undefined1 local_19;
  undefined4 local_18;
  undefined2 local_14;
  undefined2 local_12;
  undefined2 local_10;
  undefined2 local_e;
  undefined2 local_c;
  
  GPIOF.BSRR._0_2_ = 0x8000;
  RCC_AHBPeriphClockCmd(8,1);
  local_20 = 0x60;
  local_1c = 2;
  local_1b = 3;
  local_1a = 0;
  local_19 = 1;
  GPIO_Init(&GPIOD,&local_20);
  GPIO_PinAFConfig(&GPIOD,5,7);
  GPIO_PinAFConfig(&GPIOD,6,7);
  RCC_APB1PeriphClockCmd(0x20000,1);
  local_18 = 0x4b00;
  local_14 = 0;
  local_12 = 0;
  local_10 = 0;
  local_c = 0;
  local_e = 0xc;
  USART_Init(&USART2,&local_18);
  USART_Cmd(&USART2,1);
  USART_ITConfig(&USART2,0x525,1);
  NVIC_EnableIRQ(0x26);
  return;
}


// ==== Board_AnalogPins @ 0801c43e  [GPIOA GPIOF]

/* PA4 thermistor, PA5 battery, PF11/PF13/PF14: analog. high */

undefined8 Board_AnalogPins(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 uStack_a;
  undefined1 local_9;
  
  local_10 = 0x30;
  local_b = 3;
  local_c = 3;
  local_9 = 0;
  GPIO_Init(&GPIOA,&local_10);
  local_10 = 0x6800;
  local_b = 3;
  local_c = 3;
  local_9 = 0;
  GPIO_Init(&GPIOF,&local_10);
  return CONCAT17(local_9,CONCAT16(uStack_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== Board_TIM2Init @ 0801c486  [TIM2]

/* prescaler 0x104, period 0xA0, update IRQ: 10.02 ms at MSI 4.194304 MHz. high */

undefined8 Board_TIM2Init(void)

{
  undefined4 unaff_r5;
  undefined2 local_10;
  undefined2 local_e;
  
  local_10 = (undefined2)unaff_r5;
  local_e = (undefined2)((uint)unaff_r5 >> 0x10);
  RCC_APB1PeriphClockCmd(1,1);
  local_10 = 0x104;
  local_e = 0;
  TIM_TimeBaseInit(0x40000000,&local_10);
  TIM_ClearFlag(0x40000000,1);
  TIM_ITConfig(0x40000000,1,1);
  TIM_Cmd(0x40000000,1);
  return CONCAT44(0xa0,CONCAT22(local_e,local_10));
}


// ==== Board_TIM3Init @ 0801c4d4  [TIM3]

/* prescaler 0x104, period 0x0F, update IRQ: 0.996 ms at MSI 4.194304 MHz. high */

undefined8 Board_TIM3Init(void)

{
  undefined4 unaff_r5;
  undefined2 local_10;
  undefined2 local_e;
  
  local_10 = (undefined2)unaff_r5;
  local_e = (undefined2)((uint)unaff_r5 >> 0x10);
  RCC_APB1PeriphClockCmd(2,1);
  local_10 = 0x104;
  local_e = 0;
  TIM_TimeBaseInit(&TIM3,&local_10);
  TIM_ClearFlag(&TIM3,1);
  TIM_ITConfig(&TIM3,1,1);
  TIM_Cmd(&TIM3,1);
  return CONCAT44(0xf,CONCAT22(local_e,local_10));
}


// ==== LowPower_Prepare @ 0801c522

/* Power_Off, SysTick off, RCC_DeInit, range 3, MSI range 0 (~65 kHz), HSI/HSE/LSE off, all pins
   analog, RTC wake-up IRQ off, EXTI wake on rotary lines. high */

undefined4 LowPower_Prepare(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  Power_Off();
  Timers_Off();
  LowPower_PVDOff();
  LowPower_GateClocks();
  SysTick_Stop();
  RCC_DeInit();
  RCC_APB1PeriphClockCmd(0x10000000,1);
  PWR_VoltageScalingConfig(0x1800);
  do {
    iVar1 = PWR_GetFlagStatus(0x10);
  } while (iVar1 != 0);
  RCC_MSIRangeConfig(0);
  RCC_SYSCLKConfig(0);
  do {
    iVar1 = RCC_GetSYSCLKSource();
  } while (iVar1 != 0);
  RTC_ITConfig(0x4000,0);
  Osc_AllOff();
  Pins_AllAnalog();
  Wake_ArmRotary();
  return unaff_r7;
}


// ==== Power_Off @ 0801c584  [GPIOA GPIOB GPIOE GPIOF GPIOH]

/* shutdown: buzzer, backlight off, RTC lines, Frontend_SetSwitches(100), analog controls low,
   BLE_Stop, ..., then PE9 low (analog VDD off); first step of App_Sleep. high */

undefined4 Power_Off(void)

{
  undefined4 unaff_r7;
  
  GPIOE.BSRR._2_2_ = 0x80;
  GPIOE.BSRR._2_2_ = 0x100;
  LCD_PowerDown();
  GPIOB.BSRR._0_2_ = 0x40;
  GPIOB.BSRR._0_2_ = 0x80;
  GPIOB.BSRR._2_2_ = 0x40;
  HY_BusIdle();
  Frontend_SetSwitches(100);
  GPIOE.BSRR._2_2_ = 0x1000;
  GPIOE.BSRR._2_2_ = 0x2000;
  GPIOE.BSRR._2_2_ = 0x20;
  GPIOA.BSRR._2_2_ = 0x800;
  GPIOA.BSRR._2_2_ = 0x1000;
  GPIOH.BSRR._2_2_ = 4;
  GPIOE.BSRR._2_2_ = 0x8000;
  GPIOF.BSRR._2_2_ = 0x40;
  BLE_Stop();
  ADC_Off();
  GPIOE.BSRR._2_2_ = 0x200;
  return unaff_r7;
}


// ==== Wake_ArmRotary @ 0801c61c

/* EXTI wake on the rotary lines except the current position (0x20008065). high */

undefined4 Wake_ArmRotary(void)

{
  undefined4 unaff_r7;
  
  RCC_AHBPeriphClockCmd(0x80,1);
  Board_RotaryKeysInput(DAT_20008065);
  RCC_APB2PeriphClockCmd(1,1);
  if (DAT_20008065 != '\x01') {
    Wake_ArmExtiLine(7);
  }
  if (DAT_20008065 != '\x02') {
    Wake_ArmExtiLine(6);
  }
  if (DAT_20008065 != '\x04') {
    Wake_ArmExtiLine(5);
  }
  if (DAT_20008065 != '\b') {
    Wake_ArmExtiLine(4);
  }
  if (DAT_20008065 != '\x10') {
    Wake_ArmExtiLine(3);
  }
  if (DAT_20008065 != ' ') {
    Wake_ArmExtiLine(2);
  }
  if (DAT_20008065 != '@') {
    Wake_ArmExtiLine(1);
  }
  if (DAT_20008065 != -0x80) {
    Wake_ArmExtiLine(0);
  }
  Wake_ArmExtiLine(8);
  Wake_ArmExtiLine(9);
  Wake_ArmExtiLine(10);
  Wake_ArmExtiLine(0xb);
  Wake_ArmExtiLine(0xc);
  Wake_ArmExtiLine(0xd);
  Wake_ArmExtiLine(0xe);
  Wake_ArmExtiLine(0xf);
  return unaff_r7;
}


// ==== Wake_ArmExtiLine @ 0801c6f4

/* (n) SYSCFG EXTI line n from port 7 (GPIOG), EXTI_Init interrupt mode rising+falling (0x0C), NVIC
   channel by line, enabled; Wake_ArmRotary per dial line 1..7. high */

undefined8
Wake_ArmExtiLine(undefined1 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined1 uVar1;
  undefined1 local_20;
  undefined1 local_1f;
  undefined1 local_1e;
  undefined1 local_1d;
  undefined4 local_1c;
  undefined4 local_18;
  
  local_20 = (undefined1)param_2;
  local_1f = (undefined1)((uint)param_2 >> 8);
  local_1e = (undefined1)((uint)param_2 >> 0x10);
  local_1d = (undefined1)((uint)param_2 >> 0x18);
  local_1c = param_3;
  local_18 = param_4;
  uVar1 = Exti_PinSource(param_1);
  SYSCFG_EXTILineConfig(7,uVar1);
  local_1c = Exti_LineMask(param_1);
  local_18 = CONCAT13((char)((uint)local_18 >> 0x18),0x10c00);
  EXTI_Init(&local_1c);
  local_20 = Exti_IrqChannel(param_1);
  local_1f = 0;
  local_1d = 1;
  local_1e = param_1;
  NVIC_Init(&local_20);
  return CONCAT44(local_1c,CONCAT13(local_1d,CONCAT12(local_1e,CONCAT11(local_1f,local_20))));
}


// ==== Exti_IrqChannel @ 0801c756

/* EXTI line -> IRQn: 0..4 -> 6..10, 5..9 -> 23 (EXTI9_5), 10..15 -> 40 (EXTI15_10). high (emulated)
    */

byte Exti_IrqChannel(byte param_1)

{
  uint uVar1;
  
  uVar1 = (uint)param_1;
  if (uVar1 == 0) {
    param_1 = 6;
  }
  else if (uVar1 == 1) {
    param_1 = 7;
  }
  else if (uVar1 == 2) {
    param_1 = 8;
  }
  else if (uVar1 == 3) {
    param_1 = 9;
  }
  else if (uVar1 == 4) {
    param_1 = 10;
  }
  else if (uVar1 - 5 < 5) {
    param_1 = 0x17;
  }
  else if (uVar1 - 10 < 6) {
    param_1 = 0x28;
  }
  return param_1;
}


// ==== Exti_LineMask @ 0801c7aa

/* line n (0..15) -> EXTI_Line bit 1 << n as a compare chain. high (emulated) */

uint Exti_LineMask(uint param_1)

{
  uint uVar1;
  
  uVar1 = param_1 & 0xff;
  if (uVar1 == 0) {
    param_1 = 1;
  }
  else if (uVar1 == 2) {
    param_1 = 4;
  }
  else if (uVar1 < 2) {
    param_1 = 2;
  }
  else if (uVar1 == 4) {
    param_1 = 0x10;
  }
  else if (uVar1 < 4) {
    param_1 = 8;
  }
  else if (uVar1 == 6) {
    param_1 = 0x40;
  }
  else if (uVar1 < 6) {
    param_1 = 0x20;
  }
  else if (uVar1 == 8) {
    param_1 = 0x100;
  }
  else if (uVar1 < 8) {
    param_1 = 0x80;
  }
  else if (uVar1 == 10) {
    param_1 = 0x400;
  }
  else if (uVar1 < 10) {
    param_1 = 0x200;
  }
  else if (uVar1 == 0xc) {
    param_1 = 0x1000;
  }
  else if (uVar1 < 0xc) {
    param_1 = 0x800;
  }
  else if (uVar1 == 0xe) {
    param_1 = 0x4000;
  }
  else if (uVar1 < 0xe) {
    param_1 = 0x2000;
  }
  else if (uVar1 == 0xf) {
    param_1 = 0x8000;
  }
  return param_1;
}


// ==== Exti_PinSource @ 0801c852

/* line n (0..15) -> EXTI_PinSource n (identity compare chain). high */

byte Exti_PinSource(byte param_1)

{
  if (param_1 == 0) {
    param_1 = 0;
  }
  else if (param_1 == 2) {
    param_1 = 2;
  }
  else if (param_1 < 2) {
    param_1 = 1;
  }
  else if (param_1 == 4) {
    param_1 = 4;
  }
  else if (param_1 < 4) {
    param_1 = 3;
  }
  else if (param_1 == 6) {
    param_1 = 6;
  }
  else if (param_1 < 6) {
    param_1 = 5;
  }
  else if (param_1 == 8) {
    param_1 = 8;
  }
  else if (param_1 < 8) {
    param_1 = 7;
  }
  else if (param_1 == 10) {
    param_1 = 10;
  }
  else if (param_1 < 10) {
    param_1 = 9;
  }
  else if (param_1 == 0xc) {
    param_1 = 0xc;
  }
  else if (param_1 < 0xc) {
    param_1 = 0xb;
  }
  else if (param_1 == 0xe) {
    param_1 = 0xe;
  }
  else if (param_1 < 0xe) {
    param_1 = 0xd;
  }
  else if (param_1 == 0xf) {
    param_1 = 0xf;
  }
  return param_1;
}


// ==== HY_BusIdle @ 0801c8ec  [GPIOF]

/* Power_Off: HY3131 bus to idle: PF1 (CS) and PF2 (DI) high, PF0 (CK) low. high */

void HY_BusIdle(void)

{
  GPIOF.BSRR._0_2_ = 2;
  GPIOF.BSRR._0_2_ = 4;
  GPIOF.BSRR._2_2_ = 1;
  return;
}


// ==== LCD_PowerDown @ 0801c908

/* Power_Off: clear + flush, LCD_Cmd(DISABLE), wait while LCD flag 1 (ENS), LCD clock (APB1 0x200)
   off. high */

undefined4 LCD_PowerDown(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  LCD_ClearShadow();
  LCD_Flush();
  LCD_Cmd(0);
  do {
    iVar1 = LCD_GetFlagStatus(1);
  } while (iVar1 == 1);
  RCC_APB1PeriphClockCmd(0x200,0);
  return unaff_r7;
}


// ==== Timers_Off @ 0801c92e  [TIM2 TIM3]

/* LowPower_Prepare: TIM2 and TIM3 disabled and their APB1 clocks off. high */

undefined4 Timers_Off(void)

{
  undefined4 unaff_r7;
  
  TIM_Cmd(0x40000000,0);
  RCC_APB1PeriphClockCmd(1,0);
  TIM_Cmd(&TIM3,0);
  RCC_APB1PeriphClockCmd(2,0);
  return unaff_r7;
}


// ==== BLE_Stop @ 0801c954  [GPIOF USART2]

/* PF15 low, USART2 off. high */

undefined4 BLE_Stop(void)

{
  undefined4 unaff_r7;
  
  GPIOF.BSRR._2_2_ = 0x8000;
  USART_ITConfig(&USART2,0x525,0);
  USART_Cmd(&USART2,0);
  RCC_APB1PeriphClockCmd(0x20000,0);
  return unaff_r7;
}


// ==== ADC_Off @ 0801c97e  [ADC_SR]

/* ADC and its clock off. high */

undefined4 ADC_Off(void)

{
  undefined4 unaff_r7;
  
  ADC_Cmd(&ADC_SR,0);
  RCC_APB2PeriphClockCmd(0x200,0);
  return unaff_r7;
}


// ==== LowPower_PVDOff @ 0801c9a4

/* PWR_PVDCmd(DISABLE) wrapper; only caller LowPower_Prepare. high */

undefined4 LowPower_PVDOff(void)

{
  undefined4 unaff_r7;
  
  PWR_PVDCmd(0);
  return unaff_r7;
}


// ==== LowPower_GateClocks @ 0801c9ae  [RCC]

/* PWR_UltraLowPowerCmd(ENABLE), then AHBENR = AHBLPENR = 5 (GPIOA + GPIOC), APB1ENR = PWR only,
   APB2ENR = 0; from LowPower_Prepare before STOP. high */

undefined4 LowPower_GateClocks(void)

{
  undefined4 unaff_r7;
  
  PWR_UltraLowPowerCmd(1);
  RCC.AHBENR = 5;
  RCC.AHBLPENR = 5;
  RCC.APB1ENR = 0x10000000;
  RCC.APB2ENR = 0;
  return unaff_r7;
}


// ==== SysTick_Stop @ 0801c9d8  [SYSTICK_CTRL]

/* SysTick off (CTRL = 0), clear PWR wake-up flag. high */

undefined4 SysTick_Stop(void)

{
  dword dVar1;
  undefined4 unaff_r7;
  
  SYSTICK_CTRL = 0;
  dVar1 = SYSTICK_CTRL;
  SYSTICK_CTRL = dVar1 & 0xfffffffd;
  PWR_ClearFlag(1);
  return unaff_r7;
}


// ==== Osc_AllOff @ 0801ca0c

/* HSI, HSE, LSE off. high */

undefined4 Osc_AllOff(void)

{
  undefined4 unaff_r7;
  
  RCC_HSICmd(0);
  RCC_HSEConfig(0);
  RCC_LSEConfig(0);
  return unaff_r7;
}


// ==== Pins_AllAnalog @ 0801ca22  [GPIOA GPIOB GPIOC GPIOD GPIOE GPIOF GPIOG GPIOH]

/* every GPIO analog (lowest leakage in STOP). high */

undefined8 Pins_AllAnalog(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 local_a;
  undefined1 local_9;
  
  Board_GPIOClocksOn();
  local_c = 3;
  local_b = 0;
  local_9 = 0;
  local_a = 0;
  local_10 = 0xffff;
  GPIO_Init(&GPIOB,&local_10);
  GPIO_Init(&GPIOC,&local_10);
  GPIO_Init(&GPIOG,&local_10);
  GPIO_Init(&GPIOH,&local_10);
  GPIO_Init(&GPIOD,&local_10);
  GPIO_Init(&GPIOE,&local_10);
  GPIO_Init(&GPIOF,&local_10);
  local_10 = 0x9fff;
  GPIO_Init(&GPIOA,&local_10);
  RCC_AHBPeriphClocksOff();
  return CONCAT17(local_9,CONCAT16(local_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== USART_Init @ 0801caec

/* library (StdPeriph): CR2 STOP, CR1 M/PCE/PS/TE/RE (mask 0xE9F3), CR3 flow (0xFCFF), BRR from
   RCC_GetClocksFreq (PCLK2 for USART1 else PCLK1, OVER8 aware); only caller BLE_Start (USART2).
   high */

void USART_Init(USART1_t *param_1,int *param_2)

{
  ushort uVar1;
  uint uVar2;
  undefined1 auStack_30 [8];
  int local_28;
  int local_24;
  
  *(ushort *)&param_1->CR2 = *(ushort *)((int)param_2 + 6) | (ushort)param_1->CR2 & 0xcfff;
  *(ushort *)&param_1->CR1 =
       *(ushort *)(param_2 + 1) | *(ushort *)(param_2 + 2) | *(ushort *)((int)param_2 + 10) |
       (ushort)param_1->CR1 & 0xe9f3;
  *(ushort *)&param_1->CR3 = *(ushort *)(param_2 + 3) | (ushort)param_1->CR3 & 0xfcff;
  RCC_GetClocksFreq(auStack_30);
  if (param_1 != &USART1) {
    local_24 = local_28;
  }
  if ((int)((uint)(ushort)param_1->CR1 << 0x10) < 0) {
    uVar2 = (uint)(local_24 * 0x19) / (uint)(*param_2 << 1);
  }
  else {
    uVar2 = (uint)(local_24 * 0x19) / (uint)(*param_2 << 2);
  }
  if ((int)((uint)(ushort)param_1->CR1 << 0x10) < 0) {
    uVar1 = (ushort)(((uVar2 % 100) * 8 + 0x32) / 100) & 7;
  }
  else {
    uVar1 = (ushort)(((uVar2 % 100) * 0x10 + 0x32) / 100) & 0xf;
  }
  *(ushort *)&param_1->BRR = uVar1 | (ushort)(uVar2 / 100 << 4);
  return;
}


// ==== USART_Cmd @ 0801cbdc

/* StdPeriph: CR1 UE (0x2000) set/clear; BLE_Start (1) / BLE_Stop (0) on USART2. library */

void USART_Cmd(int param_1,char param_2)

{
  if (param_2 == '\0') {
    *(ushort *)(param_1 + 0xc) = *(ushort *)(param_1 + 0xc) & 0xdfff;
  }
  else {
    *(ushort *)(param_1 + 0xc) = *(ushort *)(param_1 + 0xc) | 0x2000;
  }
  return;
}


// ==== USART_SendData @ 0801cbf8

/* StdPeriph: DR = data & 0x1FF; caller 0800d170. library */

void USART_SendData(int param_1,ushort param_2)

{
  *(ushort *)(param_1 + 4) = param_2 & 0x1ff;
  return;
}


// ==== USART_ReceiveData @ 0801cc00

/* StdPeriph: DR & 0x1FF; USART2_IRQHandler RX path. library */

ushort USART_ReceiveData(int param_1)

{
  return *(ushort *)(param_1 + 4) & 0x1ff;
}


// ==== USART_ITConfig @ 0801cc08

/* library (StdPeriph): (USARTx, it, state) set/clear 1 << (it & 31) in CR1/CR2/CR3 by it >> 5; RXNE
   0x525, TXE 0x727 here (0x626 would be TC). high */

void USART_ITConfig(int param_1,ushort param_2,char param_3)

{
  byte bVar1;
  uint *puVar2;
  uint uVar3;
  
  bVar1 = (byte)param_2 >> 5;
  uVar3 = 1 << (param_2 & 0x1f);
  if (bVar1 == 1) {
    puVar2 = (uint *)(param_1 + 0xc);
  }
  else if (bVar1 == 2) {
    puVar2 = (uint *)(param_1 + 0x10);
  }
  else {
    puVar2 = (uint *)(param_1 + 0x14);
  }
  if (param_3 == '\0') {
    *puVar2 = *puVar2 & ~uVar3;
  }
  else {
    *puVar2 = *puVar2 | uVar3;
  }
  return;
}


// ==== USART_GetFlagStatus @ 0801cc5e

/* StdPeriph: SR & flag (0x80 TXE from 0800d170). library */

bool USART_GetFlagStatus(ushort *param_1,ushort param_2)

{
  return (*param_1 & param_2) != 0;
}


// ==== USART_GetITStatus @ 0801cc7c

/* library (StdPeriph): enable bit in CRx and flag bit (it >> 8) in SR both set -> 1. high */

undefined1 USART_GetITStatus(ushort *param_1,ushort param_2)

{
  byte bVar1;
  undefined1 uVar2;
  uint uVar3;
  
  bVar1 = (byte)param_2 >> 5;
  uVar3 = 1 << (param_2 & 0x1f);
  if (bVar1 == 1) {
    uVar3 = uVar3 & param_1[6];
  }
  else if (bVar1 == 2) {
    uVar3 = uVar3 & param_1[8];
  }
  else {
    uVar3 = uVar3 & param_1[10];
  }
  if ((uVar3 == 0) || ((1 << (uint)(param_2 >> 8) & (uint)*param_1) == 0)) {
    uVar2 = 0;
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== USART_ClearITPendingBit @ 0801ccec

/* StdPeriph: SR = ~(1 << (it >> 8)); USART2_IRQHandler passes 0x525 RXNE and 0x727 (StdPeriph TXE,
   not TC: TC is 0x626; a write to read-only TXE does nothing). library */

void USART_ClearITPendingBit(ushort *param_1,ushort param_2)

{
  *param_1 = ~(ushort)(1 << (uint)(param_2 >> 8));
  return;
}


// ==== CalFile_Save @ 0801cd14

/* cal.bin to SD (FatFs, 1200 B) [video] */

char CalFile_Save(void)

{
  undefined1 uVar1;
  int iVar2;
  uint uVar3;
  char cVar4;
  
  for (uVar3 = 0; uVar3 < 0x4b0; uVar3 = uVar3 + 1) {
    uVar1 = CalFile_EeRead(uVar3);
    (&DAT_20001240)[uVar3] = uVar1;
  }
  Board_ClockSelect(1);
  iVar2 = SD_MountRetry(10);
  cVar4 = iVar2 != 0;
  if (!(bool)cVar4) {
    f_mount(0,&DAT_20006830);
    iVar2 = CalFile_CreateRetry(10);
    if (iVar2 != 0) {
      cVar4 = false;
    }
  }
  if (((bool)cVar4 == false) &&
     ((DAT_200080f4 = f_write(&DAT_20006a60,&DAT_20001240,0x4b0,&DAT_20007ed8), DAT_200080f4 != '\0'
      || (DAT_20007ed8 != 0x4b0)))) {
    cVar4 = '\a';
  }
  f_close(&DAT_20006a60);
  f_open(&DAT_20006a60,"cal.bin",1);
  DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0x200,&DAT_20007ed4);
  if ((DAT_200080f4 != '\0') || (DAT_20007ed4 != 0x200)) {
    cVar4 = '\b';
  }
  if (cVar4 == '\0') {
    for (uVar3 = 0; uVar3 < 0x200; uVar3 = uVar3 + 1) {
      if ((&DAT_20001240)[uVar3] != (&DAT_20006ea8)[uVar3]) {
        cVar4 = '\t';
        break;
      }
    }
  }
  if (cVar4 == '\0') {
    DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0x200,&DAT_20007ed4);
    if ((DAT_200080f4 != '\0') || (DAT_20007ed4 != 0x200)) {
      cVar4 = '\b';
    }
    if (cVar4 == '\0') {
      for (uVar3 = 0; uVar3 < 0x200; uVar3 = uVar3 + 1) {
        if ((&DAT_20001440)[uVar3] != (&DAT_20006ea8)[uVar3]) {
          cVar4 = '\t';
          break;
        }
      }
    }
  }
  if (cVar4 == '\0') {
    DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0xb0,&DAT_20007ed4);
    if ((DAT_200080f4 != '\0') || (DAT_20007ed4 != 0xb0)) {
      cVar4 = '\b';
    }
    if (cVar4 == '\0') {
      for (uVar3 = 0; uVar3 < 0xb0; uVar3 = uVar3 + 1) {
        if ((&DAT_20001640)[uVar3] != (&DAT_20006ea8)[uVar3]) {
          cVar4 = '\t';
          break;
        }
      }
    }
  }
  File_SetTimestamp(1);
  SD_PowerOFF();
  Board_ClockSelect(0);
  return cVar4;
}


// ==== CalFile_Load @ 0801cf06

/* cal.bin from SD [video] */

char CalFile_Load(void)

{
  char cVar1;
  int iVar2;
  char cVar3;
  uint uVar4;
  bool bVar5;
  char local_4c0 [1200];
  
  Board_ClockSelect(1);
  iVar2 = SD_MountRetry(10);
  bVar5 = iVar2 != 0;
  if (!bVar5) {
    f_mount(0,&DAT_20006830);
  }
  iVar2 = f_open(&DAT_20006a60,"cal.bin",1);
  cVar3 = iVar2 == 0 && bVar5;
  if (iVar2 != 0 || !bVar5) {
    DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0x200,&DAT_20007ed4);
    if ((DAT_200080f4 == '\0') && (DAT_20007ed4 == 0x200)) {
      for (uVar4 = 0; uVar4 < 0x200; uVar4 = uVar4 + 1) {
        (&DAT_20001240)[uVar4] = (&DAT_20006ea8)[uVar4];
      }
    }
    else {
      cVar3 = '\b';
    }
  }
  if (cVar3 == '\0') {
    DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0x200,&DAT_20007ed4);
    if ((DAT_200080f4 == '\0') && (DAT_20007ed4 == 0x200)) {
      for (uVar4 = 0; uVar4 < 0x200; uVar4 = uVar4 + 1) {
        (&DAT_20001440)[uVar4] = (&DAT_20006ea8)[uVar4];
      }
    }
    else {
      cVar3 = '\b';
    }
  }
  if (cVar3 == '\0') {
    DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,0xb0,&DAT_20007ed4);
    if ((DAT_200080f4 == '\0') && (DAT_20007ed4 == 0xb0)) {
      for (uVar4 = 0; uVar4 < 0xb0; uVar4 = uVar4 + 1) {
        (&DAT_20001640)[uVar4] = (&DAT_20006ea8)[uVar4];
      }
    }
    else {
      cVar3 = '\b';
    }
  }
  f_close(&DAT_20006a60);
  SD_PowerOFF();
  Board_ClockSelect(0);
  if (cVar3 == '\0') {
    for (uVar4 = 0; uVar4 < 0x4b0; uVar4 = uVar4 + 1) {
      CalFile_EeWrite(uVar4,(&DAT_20001240)[uVar4]);
    }
    for (uVar4 = 0; uVar4 < 0x4b0; uVar4 = uVar4 + 1) {
      cVar1 = CalFile_EeRead(uVar4);
      local_4c0[uVar4] = cVar1;
    }
    for (uVar4 = 0; uVar4 < 0x4b0; uVar4 = uVar4 + 1) {
      if (local_4c0[uVar4] != (&DAT_20001240)[uVar4]) {
        return '\t';
      }
    }
  }
  return cVar3;
}


// ==== CalFile_CreateRetry @ 0801d0d6

/* (tries) path "0:", up to n f_open("cal.bin", FA_CREATE_ALWAYS | FA_WRITE) into 0x20006a60; 0 ok,
   1 failed. high */

undefined1 CalFile_CreateRetry(byte param_1)

{
  int iVar1;
  byte bVar2;
  
  DAT_200080f5 = 0x30;
  DAT_200080f6 = 0x3a;
  DAT_200080f7 = 0;
  bVar2 = 0;
  while( true ) {
    if (param_1 <= bVar2) {
      return 1;
    }
    iVar1 = f_open(&DAT_20006a60,"cal.bin",10);
    if (iVar1 == 0) break;
    bVar2 = bVar2 + 1;
  }
  return 0;
}


// ==== Log_WriteRetry @ 0801d126

/* Log_Write up to 20 times; then bumps a part counter and switches to a new file MMDDNNPP.csv (no
   header), 20 more tries; then flag 0x20008062. high */

undefined8 Log_WriteRetry(void)

{
  char cVar1;
  int iVar2;
  undefined4 in_r3;
  byte bVar3;
  uint uVar4;
  
  bVar3 = 0;
  DAT_200080e5 = '\x14';
  do {
    iVar2 = Log_Write();
    if (iVar2 == 0) goto LAB_0801d156;
    DAT_200080e5 = DAT_200080e5 + -1;
  } while (DAT_200080e5 != '\0');
  bVar3 = 1;
LAB_0801d156:
  uVar4 = (uint)bVar3;
  if (uVar4 == 1) {
    uVar4 = 0;
    Log_BeginNextPart();
    DAT_200080e5 = '\x14';
    do {
      cVar1 = Log_Write();
      if (cVar1 == '\0') goto LAB_0801d1a2;
      DAT_200080e5 = DAT_200080e5 + -1;
    } while (DAT_200080e5 != '\0');
    DAT_20008062 = 1;
    uVar4 = 1;
    DAT_2000801b = cVar1;
  }
LAB_0801d1a2:
  return CONCAT44(in_r3,uVar4);
}


// ==== Log_Write @ 0801d1a8

/* HSE on, f_mount, new file (header) or append the pending rows (several per call), read-back check
   (Log_VerifyWrite), close, MSI back; MAX/MIN on stop; stops logging after 600,000 rows (row
   counter 0x20007d40 > 599999). medium */

char Log_Write(void)

{
  bool bVar1;
  char cVar2;
  int iVar3;
  byte bVar4;
  
  bVar1 = false;
  cVar2 = '\0';
  Board_ClockSelect(1);
  iVar3 = SD_MountRetry(10);
  if (iVar3 == 0) {
    f_mount(0,&DAT_20006830);
    if (DAT_20008024 == '\0') {
      cVar2 = Log_OpenAppend(0x14);
    }
    else {
      DAT_20007ee4 = 0;
      DAT_200080f4 = Log_CreateNewFile(10);
      if (DAT_200080f4 == '\0') {
        DAT_20008024 = '\0';
        bVar1 = true;
        if (DAT_20007fae == 0) {
          DAT_20008026 = '\x01';
        }
      }
      else {
        cVar2 = '\x03';
      }
    }
    if ((cVar2 == '\0') && (iVar3 = Log_WritePendingRows(), iVar3 != 0)) {
      cVar2 = '\a';
    }
    if (bVar1) {
      File_SetTimestamp(0);
    }
    else {
      f_close(&DAT_20006a60);
    }
    if (cVar2 == '\0') {
      DAT_20007ee0 = File_Tell(&DAT_20006a60);
      cVar2 = Log_VerifyWrite();
      if (cVar2 != '\0') {
        DAT_20007ee0 = DAT_20007edc;
        DAT_200080f6 = 1;
        if (0x3b < DAT_20007d40) {
          DAT_20007d40 = DAT_20007d40 - 0x3c;
          DAT_20007d34 = DAT_20007d34 + -1;
        }
      }
      if ((cVar2 == '\0') && (DAT_20008026 != '\0')) {
        DAT_20008026 = '\0';
      }
    }
    if ((cVar2 == '\0') && (599999 < DAT_20007d40)) {
      Log_Stop(0);
      DAT_20007fe1 = 0;
      LCD_DrawMemIcon(1);
      DAT_2000801d = 1;
      DAT_20008062 = 1;
    }
    else if (DAT_2000803b != '\0') {
      Log_Stop(0);
      DAT_20007fe1 = 0;
      LCD_DrawMemIcon(1);
      DAT_2000801b = 100;
      DAT_20008062 = 1;
    }
    if ((cVar2 == '\0') && (DAT_20008025 != '\0')) {
      DAT_20008025 = '\0';
      bVar4 = 0;
      while ((bVar4 < 0x14 &&
             ((cVar2 = Log_OpenAppend(0x14), cVar2 != '\0' ||
              (iVar3 = Log_WriteMaxMinRows(), iVar3 != 0))))) {
        bVar4 = bVar4 + 1;
      }
      cVar2 = '\0';
      File_SetTimestamp(0);
    }
    SD_PowerOFF();
    Board_ClockSelect(0);
  }
  else {
    cVar2 = '\x01';
  }
  return cVar2;
}


// ==== File_SetTimestamp @ 0801d37c

/* arg 1 = "cal.bin", 0 = current log name (0x20007ec8): f_close(0x20006a60), then up to 10 tries of
   f_utime (0802256e) with date/time from the clock-chip copy (0x200080d7.., year+20 = since 1980,
   seconds/2) and f_stat (08022526) read-back compare; 1 = stamped. Needed because get_fattime
   (0802402e) is a constant 0x3449B3C0 = 2006-02-09 22:30:00, which stays as the creation time. high
   (emulated) */

undefined8 File_SetTimestamp(char param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  ushort uVar1;
  ushort uVar2;
  undefined4 uVar3;
  byte bVar4;
  
  f_close(&DAT_20006a60);
  bVar4 = 0;
  do {
    if (9 < bVar4) {
      uVar3 = 0;
LAB_0801d48a:
      return CONCAT44(param_4,uVar3);
    }
    uVar1 = (DAT_200080d7 + 0x14) * 0x200 | (ushort)DAT_200080d8 << 5 | (ushort)DAT_200080d9;
    uVar2 = (ushort)DAT_200080da * 0x800 | (ushort)DAT_200080db << 5 | (ushort)(DAT_200080dc >> 1);
    DAT_20007b3c = uVar1;
    DAT_20007b3e = uVar2;
    if (param_1 == '\0') {
      DAT_200080f4 = f_utime(DAT_20007ec8,&DAT_20007b38);
    }
    else {
      DAT_200080f4 = f_utime("cal.bin",&DAT_20007b38);
    }
    if (DAT_200080f4 == '\0') {
      if (param_1 == '\0') {
        DAT_200080f4 = f_stat(DAT_20007ec8,&DAT_20007b38);
      }
      else {
        DAT_200080f4 = f_stat("cal.bin",&DAT_20007b38);
      }
      if (((DAT_200080f4 == '\0') && (uVar1 == DAT_20007b3c)) && (uVar2 == DAT_20007b3e)) {
        uVar3 = 1;
        goto LAB_0801d48a;
      }
    }
    bVar4 = bVar4 + 1;
  } while( true );
}


// ==== Log_OpenAppend @ 0801d48c

/* f_open(log name 0x20007ec8, FA_WRITE) up to 21 tries (else 4), f_lseek to 0x20007ee0 up to 21
   tries (else 5), file size -> 0x20007edc; arg 0x14 from Log_Write is ignored. high */

undefined1 Log_OpenAppend(void)

{
  undefined1 uVar1;
  byte bVar2;
  
  uVar1 = 0;
  bVar2 = 0;
  do {
    DAT_200080f4 = f_open(&DAT_20006a60,DAT_20007ec8,2);
    if (DAT_200080f4 == '\0') goto LAB_0801d4c4;
    bVar2 = bVar2 + 1;
  } while (bVar2 < 0x15);
  uVar1 = 4;
LAB_0801d4c4:
  if (DAT_200080f4 == '\0') {
    bVar2 = 0;
    DAT_200080f4 = '\0';
    do {
      DAT_200080f4 = f_lseek(&DAT_20006a60,DAT_20007ee0);
      if (DAT_200080f4 == '\0') {
        DAT_200080f6 = 0;
        DAT_20007edc = File_Tell(&DAT_20006a60);
        return uVar1;
      }
      bVar2 = bVar2 + 1;
    } while (bVar2 < 0x15);
    uVar1 = 5;
  }
  return uVar1;
}


// ==== Log_BeginNextPart @ 0801d51a

/* from Log_WriteRetry after 20 failed tries: 0x2000801c = 1 (file open request), 0x20008024 = 1
   (Log_Write creates a new file), part number 0x20007fae++ (Log_MakePartName). high */

void Log_BeginNextPart(void)

{
  DAT_2000801c = 1;
  DAT_20008024 = 1;
  DAT_20007fae = DAT_20007fae + 1;
  return;
}


// ==== Log_CreateRetry @ 0801d53a

/* (copy, tries) 0: f_open(log, FA_CREATE_ALWAYS | FA_WRITE); 1: second FIL 0x20006c84 create R/W,
   then 0x20007ecc opened FA_READ; 0 ok else last FRESULT in 0x200080f4. medium */

char Log_CreateRetry(char param_1,byte param_2)

{
  byte bVar1;
  
  bVar1 = 0;
  while( true ) {
    if (param_2 <= bVar1) {
      return DAT_200080f4;
    }
    if (param_1 == '\0') {
      DAT_200080f4 = f_open(&DAT_20006a60,DAT_20007ec8,10);
    }
    else {
      DAT_200080f4 = f_open(&DAT_20006c84,DAT_20007ec8,0xb);
      if (DAT_200080f4 == '\0') {
        DAT_200080f4 = f_open(&DAT_20006a60,DAT_20007ecc,1);
      }
    }
    if (DAT_200080f4 == '\0') break;
    bVar1 = bVar1 + 1;
  }
  return '\0';
}


// ==== Log_CreateNewFile @ 0801d5b0

/* (tries) path "0:", Log_FindNextIndex, name from index (0x0801d838) or 0x0801d79c when 0x20007fae,
   Log_CreateRetry(0, 10); returns FRESULT. high */

undefined8 Log_CreateNewFile(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  
  DAT_200080f5 = 0x30;
  DAT_200080f6 = 0x3a;
  DAT_200080f7 = 0;
  for (bVar1 = 0; bVar1 < param_1; bVar1 = bVar1 + 1) {
    DAT_200080f4 = Log_FindNextIndex(&DAT_200080f5);
    if (DAT_200080f4 == 0) {
      if (DAT_20007fae == 0) {
        Log_MakeFileName(DAT_20007ed0);
      }
      else {
        Log_MakePartName();
      }
      DAT_200080f4 = Log_CreateRetry(0,10);
      if (DAT_200080f4 == 0) break;
    }
  }
  return CONCAT44(param_4,(uint)DAT_200080f4);
}


// ==== Log_FindNextIndex @ 0801d640

/* f_opendir(path "0:"), f_readdir loop over *.csv/*.CSV named YYMMDDNN: for names matching today's
   date (0x200080d7..d9) keeps max NN+1 in 0x20007ed0 (none = 0, >99 wraps to 0); skipped when
   0x20007fae != 0; returns FRESULT. high (emulated) */

char Log_FindNextIndex(undefined4 param_1)

{
  int iVar1;
  int iVar2;
  
  DAT_200080f4 = f_opendir(&DAT_20007b1c,param_1);
  if ((DAT_200080f4 == '\0') && (iVar2 = 0, DAT_20007fae == 0)) {
    while( true ) {
      DAT_200080f4 = 0;
      DAT_200080f4 = f_readdir(&DAT_20007b1c,&DAT_20007b38);
      if ((DAT_200080f4 != '\0') || (DAT_20007b41 == '\0')) break;
      if ((DAT_20007b41 != '.') &&
         ((((DAT_20007b4a == 'c' && (DAT_20007b4b == 's')) && (DAT_20007b4c == 'v')) ||
          (((DAT_20007b4a == 'C' && (DAT_20007b4b == 'S')) && (DAT_20007b4c == 'V')))))) {
        DAT_200080e2 = DAT_20007b41 * '\n' + DAT_20007b42 + -0x10;
        DAT_200080e3 = DAT_20007b43 * '\n' + DAT_20007b44 + -0x10;
        DAT_200080e4 = DAT_20007b45 * '\n' + DAT_20007b46 + -0x10;
        if (((DAT_200080e2 == DAT_200080d7) && (DAT_200080e3 == DAT_200080d8)) &&
           ((DAT_200080e4 == DAT_200080d9 &&
            (iVar1 = (uint)DAT_20007b47 * 10 + (uint)DAT_20007b48 + -0x20f, iVar2 < iVar1)))) {
          iVar2 = iVar1;
        }
      }
    }
    DAT_20007ed0 = iVar2;
    if (99 < iVar2) {
      DAT_20007ed0 = 0;
    }
  }
  return DAT_200080f4;
}


// ==== Log_MakePartName @ 0801d79c

/* rollover-part file name in 0x20007bd0 (name ptr 0x20007ec8): on part 1 shifts "YYMMDDNN" left two
   chars (drops YY), then writes part number 0x20007fae as the last two digits -> "MMDDNNPP.csv";
   part counter bumped by 0801d51a when Log_WriteRetry's first 20 tries fail, so the next 20 go to
   this new file (no header: Log_Write sets the header flag only for part 0). medium */

void Log_MakePartName(void)

{
  if (DAT_20007fae == 1) {
    DAT_20007bd0 = DAT_20007bd2;
    DAT_20007bd1 = DAT_20007bd3;
    DAT_20007bd2 = DAT_20007bd4;
    DAT_20007bd3 = DAT_20007bd5;
    DAT_20007bd4 = DAT_20007bd6;
    DAT_20007bd5 = DAT_20007bd7;
  }
  DAT_20007bd6 = (char)(DAT_20007fae / 10) + '0';
  DAT_20007bd7 = (char)DAT_20007fae + (char)(DAT_20007fae / 10) * -10 + '0';
  DAT_20007ec8 = &DAT_20007bd0;
  return;
}


// ==== Log_MakeFileName @ 0801d838

/* new log name "YYMMDDNN.csv" in 0x20007bd0 from the clock-chip date 0x200080d7..d9 and index arg
   (0x20007ed0 from Log_FindNextIndex); sets name ptr 0x20007ec8; lowercase ext, create_name
   upper-cases it on disk. high */

void Log_MakeFileName(int param_1)

{
  DAT_20007bd0 = DAT_200080d7 / 10 + 0x30;
  DAT_20007bd1 = DAT_200080d7 % 10 + 0x30;
  DAT_20007bd2 = DAT_200080d8 / 10 + 0x30;
  DAT_20007bd3 = DAT_200080d8 % 10 + 0x30;
  DAT_20007bd4 = DAT_200080d9 / 10 + 0x30;
  DAT_20007bd5 = DAT_200080d9 % 10 + 0x30;
  DAT_20007bd6 = (char)(param_1 / 10) + '0';
  DAT_20007bd7 = (char)param_1 + (char)(param_1 / 10) * -10 + '0';
  DAT_20007bd8 = 0x2e;
  DAT_20007bd9 = 99;
  DAT_20007bda = 0x73;
  DAT_20007bdb = 0x76;
  DAT_20007ec8 = &DAT_20007bd0;
  return;
}


// ==== Log_WritePendingRows @ 0801d8fc

/* Log_Write body: new file (0x20008026) -> Log_WriteHeader; else one Log_WriteDataRow per pending
   record (count 0x20007d44, row number 0x20007d40++) with Log_TrackMaxMin after each; on error
   rewinds row number to 0x20007d48 and sets 0x200080f6; full ring (60) bumps 0x20007d34; clears
   shadow index 0x20007fda. high */

undefined8 Log_WritePendingRows(void)

{
  byte bVar1;
  uint uVar2;
  undefined4 in_r3;
  
  DAT_20007fda = 0;
  if (DAT_20008026 == '\0') {
    DAT_20007d48 = DAT_20007d40;
    for (uVar2 = 0; (uVar2 & 0xffff) < DAT_20007d44; uVar2 = (uVar2 & 0xffff) + 1) {
      DAT_20007d40 = DAT_20007d40 + 1;
      bVar1 = Log_WriteDataRow(uVar2 & 0xffff);
      if (bVar1 != 0) {
        DAT_200080f6 = 1;
        DAT_20007d40 = DAT_20007d48;
        uVar2 = (uint)bVar1;
        goto LAB_0801d9a4;
      }
      Log_TrackMaxMin();
    }
    DAT_20007d48 = 0;
    if (DAT_20007d44 == 0x3c) {
      DAT_20007d34 = DAT_20007d34 + 1;
    }
    DAT_20007d44 = 0;
  }
  else {
    bVar1 = Log_WriteHeader();
    if (bVar1 != 0) {
      uVar2 = (uint)bVar1;
      goto LAB_0801d9a4;
    }
  }
  uVar2 = 0;
LAB_0801d9a4:
  return CONCAT44(in_r3,uVar2);
}


// ==== Log_TrackMaxMin @ 0801d9ac

/* after each row: row 1 seeds MAX (0801db34) and MIN (0801db64); later rows call Log_CheckMax /
   Log_CheckMin unless the stored extreme is already +OL (max) / -OL (min); record flags bit7 = OL,
   bit6 = negative. high */

undefined4 Log_TrackMaxMin(void)

{
  undefined4 in_r3;
  
  if (DAT_20007d40 == 1) {
    Log_SeedMax();
  }
  else if (((DAT_20008029 & 0x80) == 0) || ((DAT_20008029 & 0x40) == 0)) {
    if ((DAT_20008029 & 0x80) == 0) {
      Log_CheckMax();
    }
  }
  else {
    Log_CheckMax();
  }
  if (DAT_20007d40 == 1) {
    Log_SeedMin();
  }
  else if (((DAT_2000802a & 0x80) == 0) || ((DAT_2000802a & 0x40) != 0)) {
    if (((DAT_2000802a & 0x80) == 0) || ((DAT_2000802a & 0x40) == 0)) {
      Log_CheckMin();
    }
  }
  else {
    Log_CheckMin();
  }
  return in_r3;
}


// ==== Log_CheckMax @ 0801da40

/* replaces MAX (0x20007d4c, flags 0x20008029) when the current value 0x20007d1c is larger;
   sign-magnitude compared via ~value when bit6 set; skips any OL sample, so +OL never becomes MAX
   after row 1. medium */

undefined4 Log_CheckMax(void)

{
  undefined4 in_r3;
  uint uVar1;
  uint uVar2;
  
  if ((((DAT_20007ff5 & 0x80) == 0) || ((DAT_20007ff5 & 0x40) == 0)) && ((DAT_20007ff5 & 0x80) == 0)
     ) {
    if ((int)((uint)DAT_20008029 << 0x19) < 0) {
      uVar2 = ~DAT_20007d4c;
    }
    else {
      uVar2 = DAT_20007d4c;
    }
    if ((int)((uint)DAT_20007ff5 << 0x19) < 0) {
      uVar1 = ~DAT_20007d1c;
    }
    else {
      uVar1 = DAT_20007d1c;
    }
    if ((int)uVar2 < (int)uVar1) {
      Log_SeedMax();
    }
  }
  return in_r3;
}


// ==== Log_CheckMin @ 0801dab2

/* replaces MIN (0x20007d50, flags 0x2000802a) when the current value is smaller; skips +OL samples
   but compares -OL samples by their raw value (asymmetric with Log_CheckMax). medium */

undefined4 Log_CheckMin(void)

{
  undefined4 in_r3;
  uint uVar1;
  uint uVar2;
  
  if (((DAT_20007ff5 & 0x80) == 0) || ((DAT_20007ff5 & 0x40) != 0)) {
    if ((int)((uint)DAT_2000802a << 0x19) < 0) {
      uVar2 = ~DAT_20007d50;
    }
    else {
      uVar2 = DAT_20007d50;
    }
    if ((int)((uint)DAT_20007ff5 << 0x19) < 0) {
      uVar1 = ~DAT_20007d1c;
    }
    else {
      uVar1 = DAT_20007d1c;
    }
    if ((int)uVar1 < (int)uVar2) {
      Log_SeedMin();
    }
  }
  return in_r3;
}


// ==== Log_SeedMax @ 0801db34

/* MAX record = current row: row number 0x20007d40 -> 0x20007d54, mode/range 0x20007ff4 ->
   0x20008027, flags 0x20007ff5 -> 0x20008029, value 0x20007d1c -> 0x20007d4c; Log_TrackMaxMin row
   1, Log_CheckMax. high */

void Log_SeedMax(void)

{
  DAT_20007d54 = DAT_20007d40;
  DAT_20008027 = DAT_20007ff4;
  DAT_20008029 = DAT_20007ff5;
  DAT_20007d4c = DAT_20007d1c;
  return;
}


// ==== Log_SeedMin @ 0801db64

/* MIN twin of Log_SeedMax: -> 0x20007d58 / 0x20008028 / 0x2000802a / 0x20007d50; Log_TrackMaxMin
   row 1, Log_CheckMin. high */

void Log_SeedMin(void)

{
  DAT_20007d58 = DAT_20007d40;
  DAT_20008028 = DAT_20007ff4;
  DAT_2000802a = DAT_20007ff5;
  DAT_20007d50 = DAT_20007d1c;
  return;
}


// ==== Log_WriteDataRow @ 0801dbf0

/* one CSV data row: number (0801e018), date 0x6E + time 0x6F, then MAIN / SUB-1 / SUB-2 groups
   (Func. 0801e048, value 0801e7fa or overload/sign fields 2/3/4, unit 0801e076; subs only for the
   modes that have them, else empty commas), Remark field 0x51/0x52/0x53 by flags
   0x20007fe8/fe7/fe9, CRLF (kind 1); 0 ok, 1 write error. high (emulated) */

undefined8
Log_WriteDataRow(undefined2 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_PutRowNumber(DAT_20007d40);
  if (iVar1 == 0) {
    Log_LoadRecord(param_1);
    iVar1 = Log_FormatRow(0x6e,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(0x6f,0,0);
      if (iVar1 == 0) {
        iVar1 = Log_PutFunc(DAT_2000801e);
        if (iVar1 == 0) {
          if ((int)((uint)DAT_20007ff5 << 0x18) < 0) {
            if ((int)((uint)DAT_20007ff5 << 0x19) < 0) {
              iVar1 = Log_FormatRow(2,0,0);
              if (iVar1 != 0) {
                uVar2 = 1;
                goto LAB_0801dff6;
              }
            }
            else {
              iVar1 = Log_FormatRow(3,0,0);
              if (iVar1 != 0) {
                uVar2 = 1;
                goto LAB_0801dff6;
              }
            }
          }
          else {
            if (((int)((uint)DAT_20007ff5 << 0x19) < 0) &&
               (iVar1 = Log_FormatRow(4,0,0), iVar1 != 0)) {
              uVar2 = 1;
              goto LAB_0801dff6;
            }
            iVar1 = Log_PutValue(DAT_2000801e,DAT_2000801f,DAT_20007d1c);
            if (iVar1 != 0) {
              uVar2 = 1;
              goto LAB_0801dff6;
            }
          }
          iVar1 = Log_FormatRow(0,0,0);
          if (iVar1 == 0) {
            iVar1 = Log_PutUnit(DAT_2000801e,DAT_2000801f);
            if (iVar1 == 0) {
              if (((((((DAT_2000801e == '\x10') || (DAT_2000801e == '\x12')) ||
                     (DAT_2000801e == '\x14')) ||
                    ((DAT_2000801e == '\x02' && (DAT_20007fe9 == '\0')))) ||
                   ((DAT_2000801e == '\x04' || ((DAT_2000801e == '\r' || (DAT_2000801e == '\x0e'))))
                   )) || (DAT_2000801e == '\x0f')) ||
                 (((DAT_2000801e == '\x16' || (DAT_2000801e == '\x17')) || (DAT_2000801e == '\x18'))
                 )) {
                iVar1 = Log_PutFunc(DAT_20008020);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                if ((int)((uint)DAT_20007ff7 << 0x18) < 0) {
                  iVar1 = Log_FormatRow(3,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
                else {
                  if (DAT_20007d20 < 0) {
                    iVar1 = Log_FormatRow(4,0,0);
                    if (iVar1 != 0) {
                      uVar2 = 1;
                      goto LAB_0801dff6;
                    }
                    DAT_20007d20 = -DAT_20007d20;
                  }
                  iVar1 = Log_PutValue(DAT_20008020,DAT_20008021,DAT_20007d20);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_PutUnit(DAT_20008020,DAT_20008021);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                if ((((DAT_2000801e == '\r') || (DAT_2000801e == '\x0e')) ||
                    ((DAT_2000801e == '\x0f' ||
                     ((DAT_2000801e == '\x16' || (DAT_2000801e == '\x17')))))) ||
                   (DAT_2000801e == '\x18')) {
                  iVar1 = Log_PutFunc(DAT_20008022);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                  if ((int)((uint)DAT_20007ff9 << 0x18) < 0) {
                    iVar1 = Log_FormatRow(3,0,0);
                    if (iVar1 != 0) {
                      uVar2 = 1;
                      goto LAB_0801dff6;
                    }
                  }
                  else {
                    if (DAT_20007d24 < 0) {
                      iVar1 = Log_FormatRow(4,0,0);
                      if (iVar1 != 0) {
                        uVar2 = 1;
                        goto LAB_0801dff6;
                      }
                      DAT_20007d24 = -DAT_20007d24;
                    }
                    iVar1 = Log_PutValue(DAT_20008022,DAT_20008023,DAT_20007d24);
                    if (iVar1 != 0) {
                      uVar2 = 1;
                      goto LAB_0801dff6;
                    }
                  }
                  iVar1 = Log_FormatRow(0,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                  iVar1 = Log_PutUnit(DAT_20008022,DAT_20008023);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
                else {
                  iVar1 = Log_FormatRow(0,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                  iVar1 = Log_FormatRow(0,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                  iVar1 = Log_FormatRow(0,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
              }
              else {
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
                iVar1 = Log_FormatRow(0,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
              }
              if (DAT_20007fe8 == '\0') {
                if (DAT_20007fe7 == '\0') {
                  if ((DAT_20007fe9 != '\0') && (iVar1 = Log_FormatRow(0x53,0,0), iVar1 != 0)) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
                else {
                  iVar1 = Log_FormatRow(0x52,0,0);
                  if (iVar1 != 0) {
                    uVar2 = 1;
                    goto LAB_0801dff6;
                  }
                }
              }
              else {
                iVar1 = Log_FormatRow(0x51,0,0);
                if (iVar1 != 0) {
                  uVar2 = 1;
                  goto LAB_0801dff6;
                }
              }
              iVar1 = Log_FormatRow(0,0,0);
              if (iVar1 == 0) {
                iVar1 = Log_FormatRow(1,0,0);
                if (iVar1 == 0) {
                  uVar2 = 0;
                }
                else {
                  uVar2 = 1;
                }
              }
              else {
                uVar2 = 1;
              }
            }
            else {
              uVar2 = 1;
            }
          }
          else {
            uVar2 = 1;
          }
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
LAB_0801dff6:
  return CONCAT44(param_4,uVar2);
}


// ==== File_Tell @ 0801e010

/* (FIL *) returns fp->fptr (+8, FatFs R0.09 FIL); log FIL 0x20006a60 -> pre-write size 0x20007edc /
   0x20007ee0 (append position) for Log_VerifyWrite. high */

undefined4 File_Tell(int param_1)

{
  return *(undefined4 *)(param_1 + 8);
}


// ==== Log_PutRowNumber @ 0801e018

/* (row) CSV row-number field: Log_FormatRow kind 100 (number) then kind 0 (comma); 0 ok, 1 write
   error; Log_WriteDataRow and MAX/MIN rows. high */

undefined8
Log_PutRowNumber(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_FormatRow(100,param_1,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0,0,0);
    if (iVar1 == 0) {
      uVar2 = 0;
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== Log_PutFunc @ 0801e048

/* (mode) CSV "Func." field: Log_FormatRow kind mode + 0x15 (mode > 0x19 -> 2); 0 ok, 1 error. high
    */

undefined8 Log_PutFunc(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  
  if (0x19 < param_1) {
    param_1 = 2;
  }
  iVar1 = Log_FormatRow(param_1 + 0x15,0,0);
  return CONCAT44(param_4,(uint)(iVar1 != 0));
}


// ==== Log_PutUnit @ 0801e076

/* unit field of a CSV group: Log_FormatRow kind 0x33..0x3D+ chosen by mode (arg1) and range (arg2),
   some by a flag (0x20007fe5, 0x20007fea); 0 ok, 1 error. medium (emulated) */

undefined4 Log_PutUnit(byte param_1,byte param_2)

{
  int iVar1;
  
  if (param_1 == 0) {
    if (DAT_20007fe5 == '\0') {
      iVar1 = Log_FormatRow(0x34,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    iVar1 = Log_FormatRow(0x33,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if ((param_1 == 2) || (param_1 < 2)) {
    iVar1 = Log_FormatRow(0x35,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if ((param_1 == 4) || (param_1 < 4)) {
    iVar1 = Log_FormatRow(0x36,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 == 6) {
    if (1 < param_2) {
      iVar1 = Log_FormatRow(0x3a,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    iVar1 = Log_FormatRow(0x39,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 < 6) {
    if (DAT_20007fea == '\0') {
      iVar1 = Log_FormatRow(0x38,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    iVar1 = Log_FormatRow(0x37,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 == 8) {
    iVar1 = Log_FormatRow(0x3c,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 < 8) {
    iVar1 = Log_FormatRow(0x3b,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 == 10) {
    iVar1 = Log_FormatRow(0x3d,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 < 10) {
    if (param_2 < 2) {
      iVar1 = Log_FormatRow(0x3d,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    if (4 < param_2) {
      iVar1 = Log_FormatRow(0x3f,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    iVar1 = Log_FormatRow(0x3e,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 == 0xc) {
    if (1 < param_2) {
      iVar1 = Log_FormatRow(0x41,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    iVar1 = Log_FormatRow(0x40,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 < 0xc) {
    iVar1 = Log_FormatRow(0x35,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 == 0xe) {
LAB_0801e280:
    iVar1 = Log_FormatRow(0x43,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 < 0xe) {
LAB_0801e26c:
    iVar1 = Log_FormatRow(0x42,0,0);
    if (iVar1 == 0) {
      return 0;
    }
    return 1;
  }
  if (param_1 != 0x10) {
    if (param_1 < 0x10) {
LAB_0801e294:
      if (param_2 != 0) {
        iVar1 = Log_FormatRow(0x44,0,0);
        if (iVar1 == 0) {
          return 0;
        }
        return 1;
      }
      iVar1 = Log_FormatRow(0x43,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    if (param_1 == 0x12) {
LAB_0801e2d4:
      iVar1 = Log_FormatRow(0x46,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
    if (0x11 < param_1) {
      if (param_1 != 0x14) {
        if (param_1 < 0x14) goto LAB_0801e2d4;
        if (param_1 == 0x16) goto LAB_0801e26c;
        if (0x15 < param_1) {
          if (param_1 != 0x18) {
            if (0x17 < param_1) {
              if (param_1 != 0x19) {
                return 0;
              }
              iVar1 = Log_FormatRow(0x36,0,0);
              if (iVar1 == 0) {
                return 0;
              }
              return 1;
            }
            goto LAB_0801e280;
          }
          goto LAB_0801e294;
        }
      }
      if (param_2 != 0) {
        iVar1 = Log_FormatRow(0x47,0,0);
        if (iVar1 == 0) {
          return 0;
        }
        return 1;
      }
      iVar1 = Log_FormatRow(0x46,0,0);
      if (iVar1 == 0) {
        return 0;
      }
      return 1;
    }
  }
  iVar1 = Log_FormatRow(0x45,0,0);
  if (iVar1 == 0) {
    return 0;
  }
  return 1;
}


// ==== Log_WriteHeader @ 0801e330

/* new-file header: file pointer (0801e010) to 0x20007edc/ee0 for the read-back check, then START,
   ID, INTERVAL, group-title and column-title lines; first error returns 1. Straight-line in the
   disassembly (the decompile's do/while is an artifact). high */

char Log_WriteHeader(void)

{
  char cVar1;
  
  DAT_20007edc = File_Tell(&DAT_20006a60);
  DAT_20007ee0 = DAT_20007edc;
  cVar1 = Log_PutStartLine();
  if (((cVar1 == '\0') && (cVar1 = Log_PutIdLine(), cVar1 == '\0')) &&
     (cVar1 = Log_PutIntervalLine(), cVar1 == '\0')) {
    do {
      cVar1 = Log_PutGroupTitles();
    } while ((cVar1 != '\0') || (cVar1 = Log_PutColumnTitles(), cVar1 != '\0'));
  }
  return cVar1;
}


// ==== Log_PutGroupTitles @ 0801e3d8

/* header line ",,,MAIN,,,SUB-1,,,SUB-2,,,Remark," (Log_FormatRow kinds 0x0B..0x0E) + CRLF (kind 1);
   0 ok, 1 error. high */

undefined4 Log_PutGroupTitles(void)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_FormatRow(0xb,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0xc,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(0xd,0,0);
      if (iVar1 == 0) {
        iVar1 = Log_FormatRow(0xe,0,0);
        if (iVar1 == 0) {
          iVar1 = Log_FormatRow(1,0,0);
          if (iVar1 == 0) {
            uVar2 = 0;
          }
          else {
            uVar2 = 1;
          }
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== Log_PutColumnTitles @ 0801e43c

/* header line "No. ,Date ,Time ," (kinds 0x0F, 0x13, 0x14) + 3x "Func. ,Value,Unit," (0x10..0x12) +
   CRLF; 0 ok, 1 error. high */

undefined8 Log_PutColumnTitles(void)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 in_r3;
  byte bVar3;
  
  iVar1 = Log_FormatRow(0xf,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0x13,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(0x14,0,0);
      if (iVar1 == 0) {
        for (bVar3 = 0; bVar3 < 3; bVar3 = bVar3 + 1) {
          iVar1 = Log_FormatRow(0x10,0,0);
          if (iVar1 != 0) {
            uVar2 = 1;
            goto LAB_0801e4d0;
          }
          iVar1 = Log_FormatRow(0x11,0,0);
          if (iVar1 != 0) {
            uVar2 = 1;
            goto LAB_0801e4d0;
          }
          iVar1 = Log_FormatRow(0x12,0,0);
          if (iVar1 != 0) {
            uVar2 = 1;
            goto LAB_0801e4d0;
          }
        }
        iVar1 = Log_FormatRow(1,0,0);
        if (iVar1 == 0) {
          uVar2 = 0;
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
LAB_0801e4d0:
  return CONCAT44(in_r3,uVar2);
}


// ==== Log_PutIdLine @ 0801e4d2

/* header line "ID,<EEPROM YYMM + meter ID>," (kinds 6, 0x68) + CRLF. high */

undefined4 Log_PutIdLine(void)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_FormatRow(6,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0x68,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(1,0,0);
      if (iVar1 == 0) {
        uVar2 = 0;
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== Log_PutStartLine @ 0801e512

/* header line "START,<date>,<time>," (kinds 5, 0x66, 0x67) + CRLF. high */

undefined4 Log_PutStartLine(void)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_FormatRow(5,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0x66,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(0x67,0,0);
      if (iVar1 == 0) {
        iVar1 = Log_FormatRow(1,0,0);
        if (iVar1 == 0) {
          uVar2 = 0;
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== Log_WriteMaxMinRows @ 0801e564

/* on log stop: "MAX" (kind 0x5B) and "MIN" (0x5C) rows: row index, Func., value or overload fields,
   unit, CRLF, mode/range unpacked from 0x20008027 / 0x20008028 (mode & 0x1F, range >> 5). high
   (emulated) */

undefined8 Log_WriteMaxMinRows(void)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 in_r3;
  
  DAT_2000801e = DAT_20008027 & 0x1f;
  DAT_2000801f = DAT_20008027 >> 5;
  iVar1 = Log_FormatRow(0x5b,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_PutRowNumber(DAT_20007d54);
    if (iVar1 == 0) {
      iVar1 = Log_PutFunc(DAT_2000801e);
      if (iVar1 == 0) {
        if ((int)((uint)DAT_20008029 << 0x18) < 0) {
          if ((int)((uint)DAT_20008029 << 0x19) < 0) {
            iVar1 = Log_FormatRow(2,0,0);
            if (iVar1 != 0) {
              uVar2 = 1;
              goto LAB_0801e73e;
            }
          }
          else {
            iVar1 = Log_FormatRow(3,0,0);
            if (iVar1 != 0) {
              uVar2 = 1;
              goto LAB_0801e73e;
            }
          }
        }
        else {
          if (((int)((uint)DAT_20008029 << 0x19) < 0) && (iVar1 = Log_FormatRow(4,0,0), iVar1 != 0))
          {
            uVar2 = 1;
            goto LAB_0801e73e;
          }
          iVar1 = Log_PutValue(DAT_2000801e,DAT_2000801f,DAT_20007d4c);
          if (iVar1 != 0) {
            uVar2 = 1;
            goto LAB_0801e73e;
          }
        }
        iVar1 = Log_FormatRow(0,0,0);
        if (iVar1 == 0) {
          iVar1 = Log_PutUnit(DAT_2000801e,DAT_2000801f);
          if (iVar1 == 0) {
            iVar1 = Log_FormatRow(1,0,0);
            if (iVar1 == 0) {
              DAT_2000801e = DAT_20008028 & 0x1f;
              DAT_2000801f = DAT_20008028 >> 5;
              iVar1 = Log_FormatRow(0x5c,0,0);
              if (iVar1 == 0) {
                iVar1 = Log_PutRowNumber(DAT_20007d58);
                if (iVar1 == 0) {
                  iVar1 = Log_PutFunc(DAT_2000801e);
                  if (iVar1 == 0) {
                    if ((int)((uint)DAT_2000802a << 0x18) < 0) {
                      if ((int)((uint)DAT_2000802a << 0x19) < 0) {
                        iVar1 = Log_FormatRow(2,0,0);
                        if (iVar1 != 0) {
                          uVar2 = 1;
                          goto LAB_0801e73e;
                        }
                      }
                      else {
                        iVar1 = Log_FormatRow(3,0,0);
                        if (iVar1 != 0) {
                          uVar2 = 1;
                          goto LAB_0801e73e;
                        }
                      }
                    }
                    else {
                      if (((int)((uint)DAT_2000802a << 0x19) < 0) &&
                         (iVar1 = Log_FormatRow(4,0,0), iVar1 != 0)) {
                        uVar2 = 1;
                        goto LAB_0801e73e;
                      }
                      iVar1 = Log_PutValue(DAT_2000801e,DAT_2000801f,DAT_20007d50);
                      if (iVar1 != 0) {
                        uVar2 = 1;
                        goto LAB_0801e73e;
                      }
                    }
                    iVar1 = Log_FormatRow(0,0,0);
                    if (iVar1 == 0) {
                      iVar1 = Log_PutUnit(DAT_2000801e,DAT_2000801f);
                      if (iVar1 == 0) {
                        iVar1 = Log_FormatRow(1,0,0);
                        if (iVar1 == 0) {
                          uVar2 = 0;
                        }
                        else {
                          uVar2 = 1;
                        }
                      }
                      else {
                        uVar2 = 1;
                      }
                    }
                    else {
                      uVar2 = 1;
                    }
                  }
                  else {
                    uVar2 = 1;
                  }
                }
                else {
                  uVar2 = 1;
                }
              }
              else {
                uVar2 = 1;
              }
            }
            else {
              uVar2 = 1;
            }
          }
          else {
            uVar2 = 1;
          }
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
LAB_0801e73e:
  return CONCAT44(in_r3,uVar2);
}


// ==== Log_PutIntervalLine @ 0801e7a8

/* header line "INTERVAL,<sec>,sec," (kinds 7, 0x65, 8) + CRLF. high */

undefined4 Log_PutIntervalLine(void)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = Log_FormatRow(7,0,0);
  if (iVar1 == 0) {
    iVar1 = Log_FormatRow(0x65,0,0);
    if (iVar1 == 0) {
      iVar1 = Log_FormatRow(8,0,0);
      if (iVar1 == 0) {
        iVar1 = Log_FormatRow(1,0,0);
        if (iVar1 == 0) {
          uVar2 = 0;
        }
        else {
          uVar2 = 1;
        }
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 1;
    }
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== Log_PutValue @ 0801e7fa

/* (mode, range, value) CSV value field: Log_FormatRow kind 200 with Display_DecimalPlaces(mode,
   range) as the decimals; returns its status. high */

undefined1 Log_PutValue(undefined1 param_1,undefined1 param_2,undefined4 param_3)

{
  undefined1 uVar1;
  
  uVar1 = Display_DecimalPlaces(param_1,param_2);
  uVar1 = Log_FormatRow(200,param_3,uVar1);
  return uVar1;
}


// ==== Log_FormatRow @ 0801e82e

/* writes ONE CSV field per call by kind (0x6E/0x6F = row date/time, 0-4 header fields); strings
   from RAM 0x20000020.. . medium */

undefined4 Log_FormatRow(short param_1,undefined4 param_2,undefined1 param_3)

{
  undefined2 uVar1;
  undefined4 uVar2;
  int iVar3;
  uint uVar4;
  char local_38;
  char local_37;
  char local_36;
  char local_35;
  char local_34;
  char local_33;
  char local_32;
  char local_31;
  char local_30;
  undefined1 local_2f;
  undefined1 local_2e;
  char local_2c;
  char local_2b;
  char local_2a;
  undefined1 local_29;
  undefined1 local_28;
  
  if (param_1 == 0) {
    uVar1 = strlen(&DAT_20000232);
    iVar3 = Log_PutBytes(&DAT_20000232,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 1) {
    uVar1 = strlen(&DAT_20000020);
    iVar3 = Log_PutBytes(&DAT_20000020,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 2) {
    uVar1 = strlen(&DAT_20000050);
    iVar3 = Log_PutBytes(&DAT_20000050,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 3) {
    uVar1 = strlen(&DAT_20000044);
    iVar3 = Log_PutBytes(&DAT_20000044,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 4) {
    uVar1 = strlen(&DAT_20000234);
    iVar3 = Log_PutBytes(&DAT_20000234,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 5) {
    uVar1 = strlen(&DAT_20000028);
    iVar3 = Log_PutBytes(&DAT_20000028,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 6) {
    uVar1 = strlen(&DAT_20000024);
    iVar3 = Log_PutBytes(&DAT_20000024,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 7) {
    uVar1 = strlen(&DAT_20000030);
    iVar3 = Log_PutBytes(&DAT_20000030,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 8) {
    uVar1 = strlen(&DAT_2000003c);
    iVar3 = Log_PutBytes(&DAT_2000003c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0xb) {
    uVar1 = strlen(&DAT_2000006c);
    iVar3 = Log_PutBytes(&DAT_2000006c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0xc) {
    uVar1 = strlen(&DAT_20000078);
    iVar3 = Log_PutBytes(&DAT_20000078,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0xd) {
    uVar1 = strlen(&DAT_20000084);
    iVar3 = Log_PutBytes(&DAT_20000084,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0xe) {
    uVar1 = strlen(&DAT_20000090);
    iVar3 = Log_PutBytes(&DAT_20000090,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0xf) {
    uVar1 = strlen(&DAT_20000098);
    iVar3 = Log_PutBytes(&DAT_20000098,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x10) {
    uVar1 = strlen(&DAT_200000b8);
    iVar3 = Log_PutBytes(&DAT_200000b8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x11) {
    uVar1 = strlen(&DAT_200000c0);
    iVar3 = Log_PutBytes(&DAT_200000c0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x12) {
    uVar1 = strlen(&DAT_200000c8);
    iVar3 = Log_PutBytes(&DAT_200000c8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x13) {
    uVar1 = strlen(&DAT_200000a0);
    iVar3 = Log_PutBytes(&DAT_200000a0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x14) {
    uVar1 = strlen(&DAT_200000ac);
    iVar3 = Log_PutBytes(&DAT_200000ac,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x15) {
    uVar1 = strlen(&DAT_200000d0);
    iVar3 = Log_PutBytes(&DAT_200000d0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x16) {
    uVar1 = strlen(&DAT_200000e0);
    iVar3 = Log_PutBytes(&DAT_200000e0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x17) {
    uVar1 = strlen(&DAT_200000d8);
    iVar3 = Log_PutBytes(&DAT_200000d8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x18) {
    uVar1 = strlen(&DAT_200000e8);
    iVar3 = Log_PutBytes(&DAT_200000e8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x19) {
    uVar1 = strlen(&DAT_200000f0);
    iVar3 = Log_PutBytes(&DAT_200000f0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1a) {
    uVar1 = strlen(&DAT_200000f8);
    iVar3 = Log_PutBytes(&DAT_200000f8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1b) {
    uVar1 = strlen(&DAT_20000100);
    iVar3 = Log_PutBytes(&DAT_20000100,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1c) {
    uVar1 = strlen(&DAT_20000108);
    iVar3 = Log_PutBytes(&DAT_20000108,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1d) {
    uVar1 = strlen(&DAT_20000110);
    iVar3 = Log_PutBytes(&DAT_20000110,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1e) {
    uVar1 = strlen(&DAT_20000118);
    iVar3 = Log_PutBytes(&DAT_20000118,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x1f) {
    uVar1 = strlen(&DAT_20000120);
    iVar3 = Log_PutBytes(&DAT_20000120,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x20) {
    uVar1 = strlen(&DAT_20000128);
    iVar3 = Log_PutBytes(&DAT_20000128,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x21) {
    uVar1 = strlen(&DAT_20000130);
    iVar3 = Log_PutBytes(&DAT_20000130,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x22) {
    uVar1 = strlen(&DAT_20000138);
    iVar3 = Log_PutBytes(&DAT_20000138,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x23) {
    uVar1 = strlen(&DAT_20000140);
    iVar3 = Log_PutBytes(&DAT_20000140,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x24) {
    uVar1 = strlen(&DAT_20000148);
    iVar3 = Log_PutBytes(&DAT_20000148,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x25) {
    uVar1 = strlen(&DAT_2000014c);
    iVar3 = Log_PutBytes(&DAT_2000014c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x26) {
    uVar1 = strlen(&DAT_20000154);
    iVar3 = Log_PutBytes(&DAT_20000154,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x27) {
    uVar1 = strlen(&DAT_2000015c);
    iVar3 = Log_PutBytes(&DAT_2000015c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x28) {
    uVar1 = strlen(&DAT_20000164);
    iVar3 = Log_PutBytes(&DAT_20000164,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x29) {
    uVar1 = strlen(&DAT_2000016c);
    iVar3 = Log_PutBytes(&DAT_2000016c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x2a) {
    uVar1 = strlen(&DAT_20000174);
    iVar3 = Log_PutBytes(&DAT_20000174,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x2b) {
    uVar1 = strlen(&DAT_2000017c);
    iVar3 = Log_PutBytes(&DAT_2000017c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x2c) {
    uVar1 = strlen(&DAT_20000184);
    iVar3 = Log_PutBytes(&DAT_20000184,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x2d) {
    uVar1 = strlen(&DAT_2000018c);
    iVar3 = Log_PutBytes(&DAT_2000018c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x2e) {
    uVar1 = strlen(&DAT_20000194);
    iVar3 = Log_PutBytes(&DAT_20000194,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x33) {
    uVar1 = strlen(&DAT_200000d8);
    iVar3 = Log_PutBytes(&DAT_200000d8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x34) {
    uVar1 = strlen(&DAT_200000e0);
    iVar3 = Log_PutBytes(&DAT_200000e0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x35) {
    uVar1 = strlen(&DAT_200001a0);
    iVar3 = Log_PutBytes(&DAT_200001a0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x36) {
    uVar1 = strlen(&DAT_2000019c);
    iVar3 = Log_PutBytes(&DAT_2000019c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x37) {
    uVar1 = strlen(&DAT_200001b0);
    iVar3 = Log_PutBytes(&DAT_200001b0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x38) {
    uVar1 = strlen(&DAT_200001b4);
    iVar3 = Log_PutBytes(&DAT_200001b4,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x39) {
    uVar1 = strlen(&DAT_200001b8);
    iVar3 = Log_PutBytes(&DAT_200001b8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3a) {
    uVar1 = strlen(&DAT_200001bc);
    iVar3 = Log_PutBytes(&DAT_200001bc,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3b) {
    uVar1 = strlen(&DAT_200001c4);
    iVar3 = Log_PutBytes(&DAT_200001c4,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3c) {
    uVar1 = strlen(&DAT_200001cc);
    iVar3 = Log_PutBytes(&DAT_200001cc,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3d) {
    uVar1 = strlen(&DAT_200001d0);
    iVar3 = Log_PutBytes(&DAT_200001d0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3e) {
    uVar1 = strlen(&DAT_200001d8);
    iVar3 = Log_PutBytes(&DAT_200001d8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x3f) {
    uVar1 = strlen(&DAT_200001e0);
    iVar3 = Log_PutBytes(&DAT_200001e0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x40) {
    uVar1 = strlen(&DAT_200001e8);
    iVar3 = Log_PutBytes(&DAT_200001e8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x41) {
    uVar1 = strlen(&DAT_200001ec);
    iVar3 = Log_PutBytes(&DAT_200001ec,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x42) {
    uVar1 = strlen(&DAT_20000138);
    iVar3 = Log_PutBytes(&DAT_20000138,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x43) {
    uVar1 = strlen(&DAT_20000140);
    iVar3 = Log_PutBytes(&DAT_20000140,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x44) {
    uVar1 = strlen(&DAT_20000148);
    iVar3 = Log_PutBytes(&DAT_20000148,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x45) {
    uVar1 = strlen(&DAT_200001a4);
    iVar3 = Log_PutBytes(&DAT_200001a4,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x46) {
    uVar1 = strlen(&DAT_200001a8);
    iVar3 = Log_PutBytes(&DAT_200001a8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x47) {
    uVar1 = strlen(&DAT_200001ac);
    iVar3 = Log_PutBytes(&DAT_200001ac,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x51) {
    uVar1 = strlen(&DAT_200001f8);
    iVar3 = Log_PutBytes(&DAT_200001f8,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x52) {
    uVar1 = strlen(&DAT_200001f0);
    iVar3 = Log_PutBytes(&DAT_200001f0,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x53) {
    uVar1 = strlen(&DAT_20000204);
    iVar3 = Log_PutBytes(&DAT_20000204,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x5b) {
    uVar1 = strlen(&DAT_2000005c);
    iVar3 = Log_PutBytes(&DAT_2000005c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x5c) {
    uVar1 = strlen(&DAT_20000064);
    iVar3 = Log_PutBytes(&DAT_20000064,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 100) {
    uVar2 = Log_UIntToStr(param_2);
    uVar1 = strlen(uVar2);
    iVar3 = Log_PutBytes(uVar2,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x65) {
    local_2c = (char)(g_log_interval / 100) + '0';
    uVar4 = (uint)g_log_interval % 100;
    local_2b = (char)(uVar4 / 10) + '0';
    local_2a = (char)uVar4 + (char)(uVar4 / 10) * -10 + '0';
    local_29 = 0x2c;
    local_28 = 0;
    uVar1 = strlen(&local_2c);
    iVar3 = Log_PutBytes(&local_2c,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x66) {
    iVar3 = Log_PutDate(DAT_20007ffb,DAT_20007ffc,DAT_20007ffd);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x67) {
    iVar3 = Log_PutTime(DAT_20007ffe,DAT_20007fff,DAT_20008000);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x68) {
    Num_SplitDigits(g_date_stamp);
    local_38 = (char)DAT_20007db4 + '0';
    local_37 = (char)DAT_20007db8 + '0';
    local_36 = (char)DAT_20007dbc + '0';
    local_35 = (char)DAT_20007dc0 + '0';
    Num_SplitDigits(g_meter_id);
    local_34 = (char)DAT_20007db0 + '0';
    local_33 = (char)DAT_20007db4 + '0';
    local_32 = (char)DAT_20007db8 + '0';
    local_31 = (char)DAT_20007dbc + '0';
    local_30 = (char)DAT_20007dc0 + '0';
    local_2f = 0x2c;
    local_2e = 0;
    uVar1 = strlen(&local_38);
    iVar3 = Log_PutBytes(&local_38,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x6e) {
    iVar3 = Log_PutDate(DAT_20008007,DAT_20008008,DAT_20008009);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 0x6f) {
    iVar3 = Log_PutTime(DAT_2000800a,DAT_2000800b,DAT_2000800c);
    if (iVar3 != 0) {
      return 1;
    }
  }
  else if (param_1 == 200) {
    uVar2 = Log_FormatDigits(param_2,param_3);
    uVar1 = strlen(uVar2);
    iVar3 = Log_PutBytes(uVar2,uVar1);
    if (iVar3 != 0) {
      return 1;
    }
  }
  return 0;
}


// ==== Log_PutTime @ 0801f4b8

/* formats "HH:MM:SS," from three bytes (strlen 08023ca0) and writes it via Log_PutBytes; callers
   pass start time 0x20007ffe.. or row time 0x2000800a..; 0 ok, 1 error. high */

undefined8 Log_PutTime(byte param_1,byte param_2,byte param_3,undefined4 param_4)

{
  undefined2 uVar1;
  int iVar2;
  undefined4 local_20;
  char local_1c;
  undefined1 local_1b;
  char local_1a;
  char local_19;
  undefined2 local_18;
  undefined2 uStack_16;
  
  local_20 = CONCAT13(param_2 / 10 + 0x30,
                      CONCAT12(0x3a,CONCAT11(param_1 % 10 + 0x30,param_1 / 10 + 0x30)));
  _local_1c = CONCAT13(param_3 % 10 + 0x30,
                       CONCAT12(param_3 / 10 + 0x30,CONCAT11(0x3a,param_2 % 10 + 0x30)));
  _local_18 = CONCAT22((short)((uint)param_4 >> 0x10),0x2c);
  uVar1 = strlen(&local_20);
  iVar2 = Log_PutBytes(&local_20,uVar1);
  return CONCAT44(local_20,(uint)(iVar2 != 0));
}


// ==== Log_PutDate @ 0801f558

/* formats "20YY/MM/DD," from three bytes and writes it via Log_PutBytes; callers pass 0x20007ffb..
   or row date 0x20008007..; 0 ok, 1 error. high */

undefined8 Log_PutDate(byte param_1,byte param_2,byte param_3)

{
  undefined2 uVar1;
  int iVar2;
  undefined4 local_20;
  undefined1 local_1c;
  char local_1b;
  char local_1a;
  undefined1 local_19;
  undefined4 local_18;
  
  local_20 = CONCAT13(param_1 % 10 + 0x30,CONCAT12(param_1 / 10 + 0x30,0x3032));
  _local_1c = CONCAT13(0x2f,CONCAT12(param_2 % 10 + 0x30,CONCAT11(param_2 / 10 + 0x30,0x2f)));
  local_18 = (uint)CONCAT12(0x2c,CONCAT11(param_3 % 10 + 0x30,param_3 / 10 + 0x30));
  uVar1 = strlen(&local_20);
  iVar2 = Log_PutBytes(&local_20,uVar1);
  return CONCAT44(local_20,(uint)(iVar2 != 0));
}


// ==== Log_PutBytes @ 0801f604

/* the CSV sink: copies n bytes into the read-back shadow 0x20001240[0x20007fda++] (u16 index, no
   bounds check), f_write to the log FIL 0x20006a60, adds n to 0x20007ee4; 1 on error or short
   write; byte loop counter, so n > 255 would never end (all callers short). high */

undefined4 Log_PutBytes(int param_1,ushort param_2)

{
  undefined4 uVar1;
  uint uVar2;
  
  for (uVar2 = 0; (uVar2 & 0xff) < (uint)param_2; uVar2 = (uVar2 & 0xff) + 1) {
    (&DAT_20001240)[DAT_20007fda] = *(undefined1 *)(param_1 + (uVar2 & 0xff));
    DAT_20007fda = DAT_20007fda + 1;
  }
  DAT_200080f4 = f_write(&DAT_20006a60,param_1,param_2,&DAT_20007ed8);
  if ((DAT_200080f4 == '\0') && (DAT_20007ed8 == param_2)) {
    DAT_20007ee4 = (uint)param_2 + DAT_20007ee4;
    uVar1 = 0;
  }
  else {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== Log_VerifyWrite @ 0801f6b4

/* read-back check after every write: f_open(log name, FA_READ), f_lseek to the pre-write size
   (0x20007edc), f_read in 512-B pieces and compare with the bytes just written (buffer 0x20001240,
   count 0x20007fda); 0 ok, 8 mismatch/error; f_close. On failure Log_Write rewinds the row counter.
   high (emulated) */

undefined1 Log_VerifyWrite(void)

{
  bool bVar1;
  uint uVar2;
  undefined1 uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  
  uVar3 = 8;
  uVar6 = 0;
  DAT_200080f4 = f_open(&DAT_20006a60,DAT_20007ec8,1);
  if (DAT_200080f4 == '\0') {
    DAT_200080f4 = f_lseek(&DAT_20006a60,DAT_20007edc);
    if (DAT_200080f4 == '\0') {
      uVar2 = (uint)DAT_20007fda;
      while( true ) {
        DAT_200080f4 = 0;
        uVar2 = uVar2 & 0xffff;
        if (uVar2 < 0x201) {
          uVar5 = uVar2;
        }
        else {
          uVar5 = 0x200;
        }
        uVar2 = uVar2 - uVar5;
        DAT_200080f4 = f_read(&DAT_20006a60,&DAT_20006ea8,uVar5,&DAT_20007ed4);
        if ((DAT_200080f4 != '\0') || (DAT_20007ed4 != uVar5)) break;
        bVar1 = false;
        for (uVar4 = 0; (uVar4 & 0xffff) < uVar5; uVar4 = (uVar4 & 0xffff) + 1) {
          uVar6 = uVar6 & 0xffff;
          if ((&DAT_20001240)[(uVar4 & 0xffff) + uVar6] != (&DAT_20006ea8)[uVar4 & 0xffff]) {
            bVar1 = true;
            break;
          }
        }
        if (!bVar1) {
          uVar2 = uVar2 & 0xffff;
          uVar3 = 0;
          if (uVar2 == 0) goto LAB_0801f798;
          uVar6 = uVar5 + uVar6;
        }
      }
      uVar3 = 8;
    }
  }
LAB_0801f798:
  f_close(&DAT_20006a60);
  return uVar3;
}


// ==== Log_UIntToStr @ 0801f7ac

/* (u32) decimal digits right to left into a buffer ending NUL at 0x20007bef; returns pointer to the
   first digit (r0); used by Log_FormatRow. high */

void Log_UIntToStr(uint param_1)

{
  char *pcVar1;
  
  pcVar1 = &DAT_20007bef;
  DAT_20007bef = 0;
  do {
    pcVar1 = pcVar1 + -1;
    *pcVar1 = (char)param_1 + (char)(param_1 / 10) * -10 + '0';
    param_1 = param_1 / 10;
  } while (param_1 != 0);
  return;
}


// ==== Log_FormatDigits @ 0801f7e0

/* value to 6 decimal digits right-to-left ending at 0x20007c56 (0x20007c50..), inserting '.' at
   position arg2 from the right (0 = none, the point does not count as a digit); used by
   Log_FormatRow. high */

void Log_FormatDigits(uint param_1,char param_2)

{
  undefined1 *puVar1;
  char cVar2;
  byte bVar3;
  
  cVar2 = '\0';
  bVar3 = 0;
  puVar1 = &DAT_20007c56;
  DAT_20007c56 = 0;
  do {
    if ((param_2 == '\0') || (param_2 != cVar2)) {
      puVar1[-1] = (char)param_1 + (char)(param_1 / 10) * -10 + '0';
      param_1 = param_1 / 10;
      bVar3 = bVar3 + 1;
    }
    else {
      puVar1[-1] = 0x2e;
    }
    puVar1 = puVar1 + -1;
    cVar2 = cVar2 + '\x01';
  } while (bVar3 < 6);
  return;
}


// ==== SD_MountRetry @ 0801f84c

/* (tries) SD_InitMountAttempt up to n times, stops at the first 0; returns last code (1 init, 2 no
   card); CalFile_Save/Load and Log_Write pass 10. high */

char SD_MountRetry(byte param_1)

{
  char cVar1;
  byte bVar2;
  
  cVar1 = '\0';
  bVar2 = 0;
  while( true ) {
    if (param_1 <= bVar2) {
      return cVar1;
    }
    cVar1 = SD_InitMountAttempt();
    if (cVar1 == '\0') break;
    bVar2 = bVar2 + 1;
  }
  return '\0';
}


// ==== SD_InitMountAttempt @ 0801f87e

/* SD_LowLevel_DeInit + SD_InitMount, up to 0x300 times while it fails; 1 = never succeeded, else
   SD_Detect == 0 -> 2, 0 ok. Because SD_InitMount only reports f_mount (always FR_OK) the loop runs
   once and code 1 is dead. high */

undefined1 SD_InitMountAttempt(void)

{
  int iVar1;
  int iVar2;
  undefined1 uVar3;
  int iVar4;
  bool bVar5;
  
  uVar3 = 0;
  iVar2 = 0x300;
  do {
    SD_LowLevel_DeInit();
    iVar1 = SD_InitMount();
    iVar4 = iVar2;
    if (iVar1 == 0) break;
    iVar4 = iVar2 + -1;
    bVar5 = iVar2 != 0;
    iVar2 = iVar4;
  } while (bVar5);
  if (iVar4 == 0) {
    uVar3 = 1;
  }
  else {
    iVar2 = SD_Detect();
    if (iVar2 == 0) {
      uVar3 = 2;
    }
  }
  return uVar3;
}


// ==== SD_InitMount @ 0801f8ba

/* SD_Init() (result discarded), f_mount(0, &FatFs 0x20006830); 1 only if f_mount fails, which R0.09
   never does for drive 0: SD init errors surface later in chk_mounted/disk_initialize. high */

undefined8 SD_InitMount(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  SD_Init();
  iVar1 = f_mount(0,&DAT_20006830);
  return CONCAT44(unaff_r7,(uint)(iVar1 != 0));
}


// ==== DATA_EEPROM_Unlock @ 0801f9fc  [Flash]

/* PEKEYR 0x89ABCDEF, 0x02030405 */

void DATA_EEPROM_Unlock(void)

{
  dword dVar1;
  
  dVar1 = Flash.PECR;
  if ((int)(dVar1 << 0x1f) < 0) {
    Flash.PEKEYR = 0x89abcdef;
    Flash.PEKEYR = 0x2030405;
  }
  return;
}


// ==== DATA_EEPROM_Lock @ 0801fa1c  [Flash]

/* PECR.PELOCK */

void DATA_EEPROM_Lock(void)

{
  dword dVar1;
  
  dVar1 = Flash.PECR;
  Flash.PECR = dVar1 | 1;
  return;
}


// ==== DATA_EEPROM_ProgramByte @ 0801fa2e  [Flash]

/* FTDW off, strb (checked in the disassembly; was misnamed ProgramWord). high */

char DATA_EEPROM_ProgramByte(undefined1 *param_1,undefined1 param_2)

{
  dword dVar1;
  char cVar2;
  
  cVar2 = FLASH_WaitForLastOperation(0x8000);
  if (cVar2 == '\x04') {
    dVar1 = Flash.PECR;
    Flash.PECR = dVar1 & 0xfffffeff;
    *param_1 = param_2;
    cVar2 = FLASH_WaitForLastOperation(0x8000);
  }
  return cVar2;
}


// ==== FLASH_OB_Unlock @ 0801fa68  [Flash]

/* OPTKEYR 0xFBEAD9C8, 0x24252627 */

undefined4 FLASH_OB_Unlock(void)

{
  dword dVar1;
  undefined4 unaff_r7;
  
  dVar1 = Flash.PECR;
  if ((int)(dVar1 << 0x1d) < 0) {
    DATA_EEPROM_Unlock();
    Flash.OPTKEYR = 0xfbead9c8;
    Flash.OPTKEYR = 0x24252627;
  }
  return unaff_r7;
}


// ==== FLASH_OB_Launch @ 0801fa8e  [Flash]

/* PECR.OBL_LAUNCH: reloads option bytes = system reset */

void FLASH_OB_Launch(void)

{
  dword dVar1;
  
  dVar1 = Flash.PECR;
  Flash.PECR = dVar1 | 0x40000;
  return;
}


// ==== FLASH_OB_BORConfig @ 0801faa0  [DAT_1ff80004 Flash]

/* writes option byte 0x1FF80004 (BOR level) */

ulonglong FLASH_OB_BORConfig(byte param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  dword dVar1;
  char cVar2;
  uint uVar3;
  undefined4 uVar4;
  
  dVar1 = Flash.OBR;
  uVar3 = dVar1 >> 0x10 & 0xf0;
  cVar2 = FLASH_WaitForLastOperation(0x8000);
  if (cVar2 == '\x04') {
    DAT_1ff80004 = ~(uVar3 | param_1) << 0x10 | uVar3 | param_1;
  }
  uVar4 = FLASH_WaitForLastOperation(0x8000);
  return CONCAT44(param_4,uVar4) & 0xffffffff000000ff;
}


// ==== FLASH_OB_GetBOR @ 0801faee  [Flash]

/* (OBR >> 16) & 0xF */

uint FLASH_OB_GetBOR(void)

{
  dword dVar1;
  
  dVar1 = Flash.OBR;
  return dVar1 >> 0x10 & 0xf;
}


// ==== FLASH_ClearFlag @ 0801fafa  [Flash]

void FLASH_ClearFlag(dword param_1)

{
  Flash.SR = param_1;
  return;
}


// ==== FLASH_GetStatus @ 0801fb00  [Flash]

undefined1 FLASH_GetStatus(void)

{
  dword dVar1;
  undefined1 uVar2;
  
  dVar1 = Flash.SR;
  if ((int)(dVar1 << 0x1f) < 0) {
    uVar2 = 1;
  }
  else {
    dVar1 = Flash.SR;
    if ((int)(dVar1 << 0x17) < 0) {
      uVar2 = 2;
    }
    else {
      dVar1 = Flash.SR;
      if ((dVar1 & 0x1e00) == 0) {
        uVar2 = 4;
      }
      else {
        uVar2 = 3;
      }
    }
  }
  return uVar2;
}


// ==== FLASH_WaitForLastOperation @ 0801fb60

ulonglong FLASH_WaitForLastOperation(int param_1)

{
  undefined1 uVar1;
  uint local_10;
  
  uVar1 = FLASH_GetStatus();
  local_10 = CONCAT31(local_10._1_3_,uVar1);
  for (; ((local_10 & 0xff) == 1 && (param_1 != 0)); param_1 = param_1 + -1) {
    uVar1 = FLASH_GetStatus();
    local_10 = CONCAT31(local_10._1_3_,uVar1);
  }
  if (param_1 == 0) {
    local_10 = CONCAT31(local_10._1_3_,5);
  }
  return CONCAT44(local_10,local_10) & 0xffffffff000000ff;
}


// ==== __aeabi_dsub @ 0801fb9c

/* library (IAR runtime): double a - b; if signs differ flips b's sign and branches into
   __aeabi_dadd (080131cc), else magnitude subtract with normalize/round. Disassembly checked (teq
   r1,r3; eormi; bmi 080131cc). high */

longlong __aeabi_dsub(uint param_1,uint param_2,uint param_3,uint param_4)

{
  bool bVar1;
  byte bVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  int iVar10;
  bool bVar11;
  
  if ((int)(param_2 ^ param_4) < 0) {
    param_4 = param_4 ^ 0x80000000;
    uVar6 = param_1 - param_3;
    iVar10 = (param_2 - param_4) - (uint)(param_3 > param_1);
    if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
      bVar11 = param_1 < uVar6;
      param_1 = param_1 - uVar6;
      param_2 = (param_2 - iVar10) - (uint)bVar11;
      bVar11 = CARRY4(param_3,uVar6);
      param_3 = param_3 + uVar6;
      param_4 = param_4 + iVar10 + (uint)bVar11;
    }
    if (param_2 << 1 < 0xffe00000 && 0x1fffff < param_4 * 2) {
      uVar6 = (param_2 >> 0x14) - (param_4 >> 0x14);
      if (0x35 < (int)uVar6) goto LAB_08013298;
      param_4 = (param_4 << 0xb | 0x80000000) >> 0xb;
    }
    else {
      if (0xffdfffff < param_2 << 1) {
        return CONCAT44(param_2,param_1);
      }
      if (param_3 == 0 && (param_4 & 0x7fffffff) == 0) {
LAB_08013298:
        return CONCAT44(param_2,param_1);
      }
      if (param_2 * 2 < 0x200001 && param_2 * -2 != -0x200000) {
        bVar11 = CARRY4(param_1,param_3);
        param_1 = param_1 + param_3;
        param_2 = param_2 + (param_4 & 0x7fffffff) + (uint)bVar11;
        goto LAB_08013298;
      }
      param_4 = param_4 & 0x7fffffff;
      uVar6 = (param_2 >> 0x14) - 1;
      if (0x34 < (int)uVar6) goto LAB_08013298;
    }
    uVar3 = (int)(param_2 << 0xb | 0x80000000) >> 0xb;
    uVar7 = uVar6 - 0x20;
    if (uVar7 == 0 || (int)uVar6 < 0x20) {
      uVar4 = param_3 >> (uVar6 & 0xff);
      uVar8 = (param_3 >> (uVar6 & 0x1f) | param_3 << 0x20 - (uVar6 & 0x1f)) ^ uVar4;
      uVar7 = param_4 >> (uVar6 & 0x1f) | param_4 << 0x20 - (uVar6 & 0x1f);
      uVar5 = (uVar7 << (uVar6 & 0xff)) >> (uVar6 & 0xff);
      uVar4 = uVar4 ^ uVar7 ^ uVar5;
      uVar6 = param_1 + uVar4;
      uVar7 = uVar3 + uVar5 + (uint)CARRY4(param_1,uVar4);
      if (!CARRY4(uVar3,uVar5) && !CARRY4(uVar3 + uVar5,(uint)CARRY4(param_1,uVar4)))
      goto LAB_0801324c;
    }
    else {
      uVar4 = param_4 >> (uVar7 & 0xff);
      uVar8 = (param_4 >> (uVar7 & 0x1f) | param_4 << 0x20 - (uVar7 & 0x1f)) ^ uVar4;
      if (param_3 != 0) {
        uVar8 = uVar8 | 1;
      }
      uVar6 = param_1 + uVar4;
      uVar7 = uVar3 + CARRY4(param_1,uVar4);
      if (!CARRY4(uVar3,(uint)CARRY4(param_1,uVar4))) goto LAB_0801324c;
    }
    uVar3 = uVar7 & 1;
    uVar7 = uVar7 >> 1;
    bVar2 = (byte)uVar6;
    uVar6 = (uint)(uVar3 != 0) << 0x1f | uVar6 >> 1;
    uVar3 = uVar8 & 1;
    uVar8 = (uint)(bVar2 & 1) << 0x1f | uVar8 >> 1;
    if (uVar3 != 0) {
      uVar8 = uVar8 | 1;
    }
LAB_0801324c:
    bVar11 = 0x80000000 < uVar8 || uVar8 + 0x80000000 < (uint)((uVar6 & 1) != 0);
    uVar3 = uVar7 + 0x100000 + (param_2 & 0xfff00000) + (uint)CARRY4(uVar6,(uint)bVar11);
    if (uVar3 * 2 < 0xffe00000) {
      return CONCAT44(uVar3,uVar6 + bVar11);
    }
    return (ulonglong)(uVar3 & 0xfff00000) << 0x20;
  }
  uVar6 = param_1 - param_3;
  if (param_2 <= param_4 && (uint)(param_3 <= param_1) <= param_2 - param_4) {
    uVar3 = (param_2 - param_4) - (uint)(param_3 > param_1) ^ 0x80000000;
    bVar11 = param_1 < uVar6;
    param_1 = param_1 - uVar6;
    param_2 = (param_2 - uVar3) - (uint)bVar11;
    bVar11 = CARRY4(param_3,uVar6);
    param_3 = param_3 + uVar6;
    param_4 = param_4 + uVar3 + (uint)bVar11;
  }
  uVar6 = param_2;
  if (param_2 << 1 < 0xffe00000 && 0x1fffff < param_4 * 2) {
    uVar3 = param_2 >> 0x14;
    uVar7 = uVar3 - (param_4 >> 0x14);
    if (0x36 < (int)uVar7) goto LAB_0801fcd8;
    if ((int)uVar7 < 2) {
      uVar6 = param_4 << 0xb | 0x80000000 | param_3 >> 0x15;
      if (uVar7 == 1) {
        param_3 = param_3 << 10;
        uVar6 = uVar6 >> 1;
      }
      else {
        param_3 = param_3 << 0xb;
      }
      uVar4 = param_1 >> 0x15;
      uVar7 = param_1 * 0x800;
      param_1 = uVar7 - param_3;
      uVar7 = ((param_2 << 0xb | 0x80000000 | uVar4) - uVar6) - (uint)(uVar7 < param_3);
      uVar6 = uVar3 << 0x14;
      if (-1 < (int)uVar7) {
        if (uVar7 == 0) {
          iVar10 = LZCOUNT(param_1);
          uVar7 = param_1 << iVar10;
          uVar6 = 0;
          if (uVar7 == 0) goto LAB_0801fcd8;
          param_1 = 0;
          iVar10 = iVar10 + 0x20;
        }
        else {
          iVar10 = LZCOUNT(uVar7);
          uVar7 = uVar7 << iVar10 | param_1 >> (0x20U - iVar10 & 0xff);
          param_1 = param_1 << iVar10;
        }
        uVar4 = -((int)param_2 >> 0x1f) | uVar3 << 0x15;
        uVar6 = uVar4 + iVar10 * -0x200000;
        uVar5 = uVar6 & 0xfffffffe;
        uVar3 = ((int)param_2 >> 0x1f) * -0x80000000;
        uVar6 = uVar6 >> 1 | uVar3;
        if (uVar4 < (uint)(iVar10 * 0x200000) || uVar5 == 0) {
          uVar5 = 0x200000 - uVar5 >> 0x15;
          uVar6 = uVar7 >> (uVar5 & 0x1f);
          uVar4 = uVar7 << 0x20 - (uVar5 & 0x1f);
          uVar7 = uVar7 >> (uVar5 & 0xff);
          param_1 = param_1 >> (uVar5 & 0xff) ^ (uVar6 | uVar4) ^ uVar7;
          uVar6 = uVar3;
        }
      }
      uVar3 = param_1 >> 0xb | uVar7 << 0x15;
      bVar11 = (param_1 >> 10 & 1) != 0 && (param_1 >> 0xb & 1) != 0;
      param_1 = uVar3 + bVar11;
      uVar6 = uVar6 + ((uVar7 & 0x7fffffff) >> 0xb) + (uint)CARRY4(uVar3,(uint)bVar11);
      goto LAB_0801fcd8;
    }
    uVar3 = (param_4 << 0xb | 0x80000000) >> 0xb;
LAB_0801fc6e:
    uVar4 = param_2 >> 0x14;
    uVar6 = (param_2 << 0xb | 0x80000000) >> 0xb;
    uVar5 = uVar7 - 0x20;
    if (uVar5 == 0 || (int)uVar7 < 0x20) {
      uVar5 = param_3 >> (uVar7 & 0xff);
      uVar9 = (param_3 >> (uVar7 & 0x1f) | param_3 << 0x20 - (uVar7 & 0x1f)) ^ uVar5;
      uVar8 = uVar3 >> (uVar7 & 0xff);
      uVar7 = uVar5 ^ (uVar3 >> (uVar7 & 0x1f) | uVar3 << 0x20 - (uVar7 & 0x1f)) ^ uVar8;
      uVar3 = (param_1 - uVar7) - (uint)(uVar9 != 0);
      uVar6 = (uVar6 - uVar8) - (uint)(param_1 <= uVar7 && (uint)(uVar9 == 0) <= param_1 - uVar7);
    }
    else {
      uVar7 = uVar3 >> (uVar5 & 0xff);
      uVar9 = (param_3 | param_3 << 0x10) >> 0x10 | uVar3 << (0x20 - uVar5 & 0xff);
      uVar3 = (param_1 - uVar7) - (uint)(uVar9 != 0);
      uVar6 = uVar6 - (param_1 <= uVar7 && (uint)(uVar9 == 0) <= param_1 - uVar7);
    }
    uVar7 = -uVar9;
    if (((uVar6 & 0x100000) == 0) && (uVar4 = uVar4 - 1, (uVar4 & 0x7ff) != 0)) {
      bVar1 = (uVar7 & 0x80000000) != 0;
      uVar7 = uVar9 * -2;
      uVar5 = uVar3 * 2;
      bVar11 = CARRY4(uVar3,uVar3);
      uVar3 = uVar3 * 2 + (uint)bVar1;
      uVar6 = uVar6 * 2 + (uint)(bVar11 || CARRY4(uVar5,(uint)bVar1));
    }
    bVar11 = 0x80000000 < uVar7 || uVar7 + 0x80000000 < (uint)((uVar3 & 1) != 0);
    param_1 = uVar3 + bVar11;
    uVar6 = (uVar6 & 0xffefffff) + uVar4 * 0x100000 + (uint)CARRY4(uVar3,(uint)bVar11);
  }
  else {
    if (0xffdfffff < param_2 * 2) {
      if (param_2 * 2 != 0xffe00000 || 0xffdfffff < param_4 << 1) {
        uVar6 = 0xffffffff;
      }
      goto LAB_0801fcd8;
    }
    uVar3 = param_4 & 0x7fffffff;
    if (param_3 != 0 || (param_4 & 0x7fffffff) != 0) {
      if (0x400000 < param_2 * 2 || param_2 * -2 == -0x400000) {
        uVar7 = (param_2 >> 0x14) - 1;
        if (0x34 < (int)uVar7) goto LAB_0801fcd8;
        goto LAB_0801fc6e;
      }
      bVar11 = param_1 < param_3;
      param_1 = param_1 - param_3;
      uVar6 = (param_2 - uVar3) - (uint)bVar11;
    }
    if (param_1 == 0 && (uVar6 & 0x7fffffff) == 0) {
      uVar6 = 0;
    }
  }
LAB_0801fcd8:
  return CONCAT44(uVar6,param_1);
}


// ==== RCC_DeInit @ 0801fd1c  [RCC]

void RCC_DeInit(void)

{
  dword dVar1;
  
  dVar1 = RCC.CR;
  RCC.CR = dVar1 | 0x100;
  dVar1 = RCC.CFGR;
  RCC.CFGR = dVar1 & 0x88ffc00c;
  dVar1 = RCC.CR;
  RCC.CR = dVar1 & 0xeefefffe;
  dVar1 = RCC.CR;
  RCC.CR = dVar1 & 0xfffbffff;
  dVar1 = RCC.CFGR;
  RCC.CFGR = dVar1 & 0xff02ffff;
  RCC.CIR = 0;
  return;
}


// ==== RCC_HSEConfig @ 0801fd7a  [RCC]

/* CR byte 2 */

void RCC_HSEConfig(undefined1 param_1)

{
  RCC.CR._2_1_ = 0;
  RCC.CR._2_1_ = param_1;
  return;
}


// ==== RCC_WaitForHSEStartUp @ 0801fd8a

/* HSERDY, 0x5000 polls */

undefined8 RCC_WaitForHSEStartUp(void)

{
  char cVar1;
  int iVar2;
  int local_10;
  
  local_10 = 0;
  do {
    cVar1 = RCC_GetFlagStatus(0x31);
    local_10 = local_10 + 1;
    if (local_10 == 0x5000) break;
  } while (cVar1 == '\0');
  iVar2 = RCC_GetFlagStatus(0x31);
  return CONCAT44(local_10,(uint)(iVar2 != 0));
}


// ==== RCC_MSIRangeConfig @ 0801fdca  [RCC]

/* ICSCR.MSIRANGE */

void RCC_MSIRangeConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = RCC.ICSCR;
  RCC.ICSCR = dVar1 & 0xffff1fff | param_1;
  return;
}


// ==== RCC_HSICmd @ 0801fde2  [DAT_42470000]

/* bit-band CR.HSION */

void RCC_HSICmd(byte param_1)

{
  DAT_42470000 = (uint)param_1;
  return;
}


// ==== RCC_LSEConfig @ 0801fdec  [RCC]

/* CSR byte 1 */

void RCC_LSEConfig(undefined1 param_1)

{
  RCC.CSR._1_1_ = 0;
  RCC.CSR._1_1_ = param_1;
  return;
}


// ==== RCC_LSICmd @ 0801fdfc  [DAT_42470680]

/* bit-band CSR.LSION */

void RCC_LSICmd(byte param_1)

{
  DAT_42470680 = (uint)param_1;
  return;
}


// ==== RCC_SYSCLKConfig @ 0801fe06  [RCC]

/* 0 MSI, 1 HSI, 2 HSE, 3 PLL */

void RCC_SYSCLKConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = RCC.CFGR;
  RCC.CFGR = dVar1 & 0xfffffffc | param_1;
  return;
}


// ==== RCC_GetSYSCLKSource @ 0801fe1e  [RCC]

uint RCC_GetSYSCLKSource(void)

{
  dword dVar1;
  
  dVar1 = RCC.CFGR;
  return dVar1 & 0xc;
}


// ==== RCC_GetClocksFreq @ 0801fe2a  [RCC]

void RCC_GetClocksFreq(uint *param_1)

{
  dword dVar1;
  dword dVar2;
  uint uVar3;
  
  dVar1 = RCC.CFGR;
  uVar3 = dVar1 & 0xc;
  if (uVar3 == 0) {
    dVar1 = RCC.ICSCR;
    *param_1 = (1 << ((dVar1 & 0xffff) >> 0xd) + 1) * 0x8000;
  }
  else if (uVar3 == 4) {
    *param_1 = 16000000;
  }
  else if (uVar3 == 8) {
    *param_1 = 8000000;
  }
  else if (uVar3 == 0xc) {
    dVar1 = RCC.CFGR;
    dVar2 = RCC.CFGR;
    uVar3 = ((dVar2 & 0xc00000) >> 0x16) + 1;
    dVar2 = RCC.CFGR;
    if ((dVar2 & 0x10000) == 0) {
      *param_1 = ((uint)(byte)(&DAT_2000020c)[(dVar1 & 0x3c0000) >> 0x12] * 16000000) / uVar3;
    }
    else {
      *param_1 = ((uint)(byte)(&DAT_2000020c)[(dVar1 & 0x3c0000) >> 0x12] * 8000000) / uVar3;
    }
  }
  else {
    dVar1 = RCC.ICSCR;
    *param_1 = (1 << ((dVar1 & 0xffff) >> 0xd) + 1) * 0x8000;
  }
  dVar1 = RCC.CFGR;
  param_1[1] = *param_1 >> (&DAT_20000218)[(dVar1 & 0xf0) >> 4];
  dVar1 = RCC.CFGR;
  param_1[2] = param_1[1] >> (&DAT_20000218)[(dVar1 & 0x700) >> 8];
  dVar1 = RCC.CFGR;
  param_1[3] = param_1[1] >> (&DAT_20000218)[(dVar1 & 0x3800) >> 0xb];
  return;
}


// ==== RCC_RTCCLKConfig @ 0801ff34  [RCC]

/* 0x20000 = LSI */

void RCC_RTCCLKConfig(uint param_1)

{
  dword dVar1;
  
  if ((param_1 & 0x30000) == 0x30000) {
    dVar1 = RCC.CR;
    RCC.CR = dVar1 & 0x9fffffff | param_1 & 0x60000000;
  }
  dVar1 = RCC.CSR;
  RCC.CSR = dVar1 & 0xfffcffff;
  dVar1 = RCC.CSR;
  RCC.CSR = dVar1 | param_1 & 0x30000;
  return;
}


// ==== RCC_RTCCLKCmd @ 0801ff70  [DAT_424706d8]

/* bit-band CSR.RTCEN */

void RCC_RTCCLKCmd(byte param_1)

{
  DAT_424706d8 = (uint)param_1;
  return;
}


// ==== RCC_RTCResetCmd @ 0801ff78  [DAT_424706dc]

/* bit-band CSR.RTCRST */

void RCC_RTCResetCmd(byte param_1)

{
  DAT_424706dc = (uint)param_1;
  return;
}


// ==== RCC_AHBPeriphClockCmd @ 0801ff80  [RCC]

void RCC_AHBPeriphClockCmd(uint param_1,char param_2)

{
  dword dVar1;
  
  if (param_2 == '\0') {
    dVar1 = RCC.AHBENR;
    RCC.AHBENR = dVar1 & ~param_1;
  }
  else {
    dVar1 = RCC.AHBENR;
    RCC.AHBENR = dVar1 | param_1;
  }
  return;
}


// ==== RCC_APB2PeriphClockCmd @ 0801ff9e  [RCC]

void RCC_APB2PeriphClockCmd(uint param_1,char param_2)

{
  dword dVar1;
  
  if (param_2 == '\0') {
    dVar1 = RCC.APB2ENR;
    RCC.APB2ENR = dVar1 & ~param_1;
  }
  else {
    dVar1 = RCC.APB2ENR;
    RCC.APB2ENR = dVar1 | param_1;
  }
  return;
}


// ==== RCC_APB1PeriphClockCmd @ 0801ffbc  [RCC]

void RCC_APB1PeriphClockCmd(uint param_1,char param_2)

{
  dword dVar1;
  
  if (param_2 == '\0') {
    dVar1 = RCC.APB1ENR;
    RCC.APB1ENR = dVar1 & ~param_1;
  }
  else {
    dVar1 = RCC.APB1ENR;
    RCC.APB1ENR = dVar1 | param_1;
  }
  return;
}


// ==== RCC_APB2PeriphResetCmd @ 0801ffda  [RCC]

void RCC_APB2PeriphResetCmd(uint param_1,char param_2)

{
  dword dVar1;
  
  if (param_2 == '\0') {
    dVar1 = RCC.APB2RSTR;
    RCC.APB2RSTR = dVar1 & ~param_1;
  }
  else {
    dVar1 = RCC.APB2RSTR;
    RCC.APB2RSTR = dVar1 | param_1;
  }
  return;
}


// ==== RCC_GetFlagStatus @ 0801fff8  [RCC]

/* flag >> 5: 1 = CR, else CSR */

bool RCC_GetFlagStatus(byte param_1)

{
  dword dVar1;
  
  if (param_1 >> 5 == 1) {
    dVar1 = RCC.CR;
  }
  else {
    dVar1 = RCC.CSR;
  }
  return (int)((dVar1 >> (param_1 & 0x1f)) << 0x1f) < 0;
}


// ==== ADC_DeInit @ 08020094  [ADC_SR]

void ADC_DeInit(dword *param_1)

{
  if (param_1 == &ADC_SR) {
    RCC_APB2PeriphResetCmd(0x200,1);
    RCC_APB2PeriphResetCmd(0x200,0);
  }
  return;
}


// ==== ADC_Init @ 080200b4

void ADC_Init(int param_1,uint *param_2)

{
  *(uint *)(param_1 + 4) =
       *(uint *)(param_1 + 4) & 0xfcfffeff | *param_2 | (uint)(byte)param_2[1] << 8;
  *(uint *)(param_1 + 8) =
       *(uint *)(param_1 + 8) & 0xc0fff7fd |
       param_2[4] | param_2[3] | param_2[2] | (uint)*(byte *)((int)param_2 + 5) << 1;
  *(uint *)(param_1 + 0x30) =
       *(uint *)(param_1 + 0x30) & 0xfe0fffff | (uint)(byte)((char)param_2[5] - 1) << 0x14;
  return;
}


// ==== ADC_Cmd @ 08020104

void ADC_Cmd(int param_1,char param_2)

{
  if (param_2 == '\0') {
    *(uint *)(param_1 + 8) = *(uint *)(param_1 + 8) & 0xfffffffe;
  }
  else {
    *(uint *)(param_1 + 8) = *(uint *)(param_1 + 8) | 1;
  }
  return;
}


// ==== ADC_RegularChannelConfig @ 0802012c

void ADC_RegularChannelConfig(int param_1,byte param_2,byte param_3,uint param_4)

{
  if (param_2 < 0x1e) {
    if (param_2 < 0x14) {
      if (param_2 < 10) {
        *(uint *)(param_1 + 0x14) =
             *(uint *)(param_1 + 0x14) & ~(7 << (uint)(byte)(param_2 * '\x03')) |
             (param_4 & 0xff) << (uint)(byte)(param_2 * '\x03');
      }
      else {
        *(uint *)(param_1 + 0x10) =
             *(uint *)(param_1 + 0x10) & ~(7 << (uint)(byte)((param_2 - 10) * '\x03')) |
             (param_4 & 0xff) << (uint)(byte)((param_2 - 10) * '\x03');
      }
    }
    else {
      *(uint *)(param_1 + 0xc) =
           *(uint *)(param_1 + 0xc) & ~(7 << (uint)(byte)((param_2 - 0x14) * '\x03')) |
           (param_4 & 0xff) << (uint)(byte)((param_2 - 0x14) * '\x03');
    }
  }
  else {
    *(uint *)(param_1 + 0x5c) =
         *(uint *)(param_1 + 0x5c) & ~(7 << (uint)(byte)((param_2 - 0x1e) * '\x03')) |
         (param_4 & 0xff) << (uint)(byte)((param_2 - 0x1e) * '\x03');
  }
  if (param_3 < 7) {
    *(uint *)(param_1 + 0x40) =
         *(uint *)(param_1 + 0x40) & ~(0x1f << (uint)(byte)((param_3 - 1) * '\x05')) |
         (uint)param_2 << (uint)(byte)((param_3 - 1) * '\x05');
  }
  else if (param_3 < 0xd) {
    *(uint *)(param_1 + 0x3c) =
         *(uint *)(param_1 + 0x3c) & ~(0x1f << (uint)(byte)((param_3 - 7) * '\x05')) |
         (uint)param_2 << (uint)(byte)((param_3 - 7) * '\x05');
  }
  else if (param_3 < 0x13) {
    *(uint *)(param_1 + 0x38) =
         *(uint *)(param_1 + 0x38) & ~(0x1f << (uint)(byte)((param_3 - 0xd) * '\x05')) |
         (uint)param_2 << (uint)(byte)((param_3 - 0xd) * '\x05');
  }
  else if (param_3 < 0x19) {
    *(uint *)(param_1 + 0x34) =
         *(uint *)(param_1 + 0x34) & ~(0x1f << (uint)(byte)((param_3 - 0x13) * '\x05')) |
         (uint)param_2 << (uint)(byte)((param_3 - 0x13) * '\x05');
  }
  else {
    *(uint *)(param_1 + 0x30) =
         *(uint *)(param_1 + 0x30) & ~(0x1f << (uint)(byte)((param_3 - 0x19) * '\x05')) |
         (uint)param_2 << (uint)(byte)((param_3 - 0x19) * '\x05');
  }
  return;
}


// ==== ADC_SoftwareStartConv @ 080202ea

void ADC_SoftwareStartConv(int param_1)

{
  *(uint *)(param_1 + 8) = *(uint *)(param_1 + 8) | 0x40000000;
  return;
}


// ==== ADC_GetConversionValue @ 080202f4

uint ADC_GetConversionValue(int param_1)

{
  return *(uint *)(param_1 + 0x58) & 0xffff;
}


// ==== ADC_GetFlagStatus @ 080202fa

bool ADC_GetFlagStatus(uint *param_1,ushort param_2)

{
  return (*param_1 & (uint)param_2) != 0;
}


// ==== ADC_GetITStatus @ 08020314

undefined1 ADC_GetITStatus(uint *param_1,ushort param_2)

{
  undefined1 uVar1;
  
  if (((*param_1 & (uint)(param_2 >> 8)) == 0) || ((param_1[1] & 1 << (param_2 & 0xff)) == 0)) {
    uVar1 = 0;
  }
  else {
    uVar1 = 1;
  }
  return uVar1;
}


// ==== ADC_ClearITPendingBit @ 08020348

void ADC_ClearITPendingBit(uint *param_1,undefined2 param_2)

{
  *param_1 = ~(uint)(byte)((ushort)param_2 >> 8);
  return;
}


// ==== __aeabi_cfrcmple @ 0802035c

/* IAR runtime float compare, reversed (flags of b <=> a; NaN -> C set, Z clear); Temp_CToF,
   LCD_SubDbm. Disassembly checked. library */

void __aeabi_cfrcmple(uint param_1,uint param_2)

{
  uint uVar1;
  
  uVar1 = param_1 * 2;
  if (uVar1 < 0xff000001) {
    uVar1 = param_2 * 2;
  }
  if ((uVar1 < 0xff000001) && (((param_1 | param_2) & 0x80000000) != 0)) {
    return;
  }
  return;
}


// ==== __aeabi_cfcmple @ 08020390

/* IAR runtime float compare (flags of a <=> b; NaN unordered); LCD_SubDbm dBm vs 0. Disassembly
   checked. library */

void __aeabi_cfcmple(uint param_1,uint param_2)

{
  uint uVar1;
  
  uVar1 = param_1 * 2;
  if (uVar1 < 0xff000001) {
    uVar1 = param_2 * 2;
  }
  if ((uVar1 < 0xff000001) && (((param_1 | param_2) & 0x80000000) != 0)) {
    return;
  }
  return;
}


// ==== LCD_Init @ 080203b4  [LCD]

/* StdPeriph LCD_Init (FCR prescaler/divider, CR duty/bias) */

void LCD_Init(uint *param_1)

{
  dword dVar1;
  
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 & 0xfc03ffff;
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 | *param_1 | param_1[1];
  LCD_WaitForSynchro();
  dVar1 = LCD.CR;
  LCD.CR = dVar1 & 0xffffff81;
  dVar1 = LCD.CR;
  LCD.CR = dVar1 | param_1[2] | param_1[3] | param_1[4];
  return;
}


// ==== LCD_Cmd @ 080203fa  [DAT_42048000]

/* bit-band CR.LCDEN */

void LCD_Cmd(byte param_1)

{
  DAT_42048000 = (uint)param_1;
  return;
}


// ==== LCD_WaitForSynchro @ 08020402  [LCD]

/* FCRSF, 0x8000 polls */

void LCD_WaitForSynchro(void)

{
  dword dVar1;
  int iVar2;
  
  iVar2 = 0;
  do {
    dVar1 = LCD.SR;
    iVar2 = iVar2 + 1;
    if (iVar2 == 0x8000) {
      return;
    }
  } while ((dVar1 & 0x20) == 0);
  return;
}


// ==== LCD_MuxSegmentCmd @ 0802041e  [DAT_4204801c]

/* bit-band CR.MUX_SEG */

void LCD_MuxSegmentCmd(byte param_1)

{
  DAT_4204801c = (uint)param_1;
  return;
}


// ==== LCD_PulseOnDurationConfig @ 08020426  [LCD]

void LCD_PulseOnDurationConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 & 0xffffff8f;
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 | param_1;
  return;
}


// ==== LCD_DeadTimeConfig @ 0802043e  [LCD]

void LCD_DeadTimeConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 & 0xfffffc7f;
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 | param_1;
  return;
}


// ==== LCD_BlinkConfig @ 08020456  [LCD]

void LCD_BlinkConfig(uint param_1,uint param_2)

{
  dword dVar1;
  
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 & 0xfffc1fff;
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 | param_2 | param_1;
  return;
}


// ==== LCD_ContrastConfig @ 08020472  [LCD]

void LCD_ContrastConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 & 0xffffe3ff;
  dVar1 = LCD.FCR;
  LCD.FCR = dVar1 | param_1;
  return;
}


// ==== LCD_UpdateDisplayRequest @ 0802048a  [DAT_42048108]

/* bit-band SR.UDR */

void LCD_UpdateDisplayRequest(void)

{
  DAT_42048108 = 1;
  return;
}


// ==== LCD_GetFlagStatus @ 08020492  [LCD]

bool LCD_GetFlagStatus(uint param_1)

{
  dword dVar1;
  
  dVar1 = LCD.SR;
  return (dVar1 & param_1) != 0;
}


// ==== PWR_RTCAccessCmd @ 0802079c  [DAT_420e0020]

/* bit-band CR.DBP */

void PWR_RTCAccessCmd(byte param_1)

{
  DAT_420e0020 = (uint)param_1;
  return;
}


// ==== PWR_PVDCmd @ 080207a4  [DAT_420e0010]

/* bit-band CR.PVDE */

void PWR_PVDCmd(byte param_1)

{
  DAT_420e0010 = (uint)param_1;
  return;
}


// ==== PWR_UltraLowPowerCmd @ 080207ac  [DAT_420e0024]

/* bit-band CR.ULP */

void PWR_UltraLowPowerCmd(byte param_1)

{
  DAT_420e0024 = (uint)param_1;
  return;
}


// ==== PWR_VoltageScalingConfig @ 080207b4  [PWR]

/* 0x800 = range 1 */

void PWR_VoltageScalingConfig(uint param_1)

{
  dword dVar1;
  
  dVar1 = PWR.CR;
  PWR.CR = (dVar1 & 0xffffe7ff | param_1) & 0xfffffff3;
  return;
}


// ==== PWR_EnterSTOPMode @ 080207cc  [PWR SCB_SCR]

/* CR.LPDS + SCB.SCR SLEEPDEEP + WFI/WFE */

void PWR_EnterSTOPMode(uint param_1,char param_2)

{
  dword dVar1;
  
  dVar1 = PWR.CR;
  PWR.CR = dVar1 & 0xfffffffc | param_1;
  dVar1 = SCB_SCR;
  SCB_SCR = dVar1 | 4;
  if (param_2 == '\x01') {
    WaitForInterrupt();
  }
  else {
    WaitForEvent();
  }
  dVar1 = SCB_SCR;
  SCB_SCR = dVar1 & 0xfffffffb;
  return;
}


// ==== PWR_GetFlagStatus @ 08020808  [PWR]

bool PWR_GetFlagStatus(uint param_1)

{
  dword dVar1;
  
  dVar1 = PWR.CSR;
  return (dVar1 & param_1) != 0;
}


// ==== PWR_ClearFlag @ 08020822  [PWR]

/* CR |= flag << 2 */

void PWR_ClearFlag(int param_1)

{
  dword dVar1;
  
  dVar1 = PWR.CR;
  PWR.CR = dVar1 | param_1 << 2;
  return;
}


// ==== DBGMCU_Config @ 08020848

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* DBGMCU_CR 0xE0042004 */

void DBGMCU_Config(uint param_1,char param_2)

{
  if (param_2 == '\0') {
    _DAT_e0042004 = _DAT_e0042004 & ~param_1;
  }
  else {
    _DAT_e0042004 = _DAT_e0042004 | param_1;
  }
  return;
}


// ==== NVIC_PriorityGroupConfig @ 0802086c  [SCB_AIRCR]

/* AIRCR | 0x05FA0000 */

void NVIC_PriorityGroupConfig(uint param_1)

{
  SCB_AIRCR = param_1 | 0x5fa0000;
  return;
}


// ==== NVIC_Init @ 08020876  [NVIC_ICER0 NVIC_IPR0 NVIC_ISER0 SCB_AIRCR]

void NVIC_Init(byte *param_1)

{
  dword dVar1;
  uint uVar2;
  
  if (param_1[3] == 0) {
    (&NVIC_ICER0)[(int)(uint)*param_1 >> 5] = 1 << (*param_1 & 0x1f);
  }
  else {
    dVar1 = SCB_AIRCR;
    uVar2 = 0x700 - (dVar1 & 0x700) >> 8;
    *(char *)((int)&NVIC_IPR0 + (uint)*param_1) =
         (char)(((uint)param_1[1] << (4 - uVar2 & 0xff) | (uint)param_1[2] & 0xf >> (uVar2 & 0xff))
               << 4);
    (&NVIC_ISER0)[(int)(uint)*param_1 >> 5] = 1 << (*param_1 & 0x1f);
  }
  return;
}


// ==== NVIC_SetVectorTable @ 080208e0  [SCB_VTOR]

/* StdPeriph: VTOR = base | (offset & 0x1FFFFF80); called with (0x08000000, 0x6000). high */

void NVIC_SetVectorTable(uint param_1,uint param_2)

{
  SCB_VTOR = param_2 & 0x1fffff80 | param_1;
  return;
}


// ==== TIM_TimeBaseInit @ 08020908

void TIM_TimeBaseInit(TIM2_t *param_1,undefined2 *param_2)

{
  ushort uVar1;
  
  uVar1 = (ushort)param_1->CR1;
  if ((((param_1 == &TIM2) || (param_1 == (TIM2_t *)&TIM3)) || (param_1 == (TIM2_t *)&TIM4)) ||
     (param_1 == (TIM2_t *)&TIM5)) {
    uVar1 = uVar1 & 0xff8f | param_2[1];
  }
  if ((param_1 != (TIM2_t *)&TIM6) && (param_1 != (TIM2_t *)&TIM7)) {
    uVar1 = uVar1 & 0xfcff | param_2[4];
  }
  *(ushort *)&param_1->CR1 = uVar1;
  param_1->ARR = *(dword *)(param_2 + 2);
  *(undefined2 *)&param_1->PSC = *param_2;
  *(undefined2 *)&param_1->EGR = 1;
  return;
}


// ==== TIM_Cmd @ 0802096c

void TIM_Cmd(ushort *param_1,char param_2)

{
  if (param_2 == '\0') {
    *param_1 = *param_1 & 0xfffe;
  }
  else {
    *param_1 = *param_1 | 1;
  }
  return;
}


// ==== TIM_ITConfig @ 08020988

/* DIER */

void TIM_ITConfig(int param_1,ushort param_2,char param_3)

{
  if (param_3 == '\0') {
    *(ushort *)(param_1 + 0xc) = *(ushort *)(param_1 + 0xc) & ~param_2;
  }
  else {
    *(ushort *)(param_1 + 0xc) = *(ushort *)(param_1 + 0xc) | param_2;
  }
  return;
}


// ==== TIM_ClearFlag @ 0802099e

/* SR = ~flag */

void TIM_ClearFlag(int param_1,ushort param_2)

{
  *(ushort *)(param_1 + 0x10) = ~param_2;
  return;
}


// ==== RTC_ITConfig @ 080209a8  [RTC]

/* RTC write-protect keys 0xCA/0x53, CR/TAFCR interrupt bits. high */

void RTC_ITConfig(uint param_1,char param_2)

{
  dword dVar1;
  
  RTC.WPR = 0xca;
  RTC.WPR = 0x53;
  if (param_2 == '\0') {
    dVar1 = RTC.CR;
    RTC.CR = dVar1 & ~(param_1 & 0xfffffffb);
    dVar1 = RTC.TAFCR;
    RTC.TAFCR = dVar1 & ~(param_1 & 4);
  }
  else {
    dVar1 = RTC.CR;
    RTC.CR = dVar1 | param_1 & 0xfffffffb;
    dVar1 = RTC.TAFCR;
    RTC.TAFCR = dVar1 | param_1 & 4;
  }
  RTC.WPR = 0xff;
  return;
}


// ==== RTC_GetITStatus @ 080209fc  [RTC]

/* StdPeriph: enabled (CR / TAFCR tamper) && pending (ISR); RTC_WKUP_IRQHandler asks for RTC_IT_WUT
   0x4000. library */

undefined1 RTC_GetITStatus(uint param_1)

{
  dword dVar1;
  dword dVar2;
  dword dVar3;
  undefined1 uVar4;
  
  dVar3 = RTC.TAFCR;
  dVar1 = RTC.CR;
  dVar2 = RTC.ISR;
  if (((dVar1 & param_1) == 0 && (dVar3 & 4 & (param_1 >> (param_1 >> 0x12 & 0xff)) >> 0xf) == 0) ||
     ((dVar2 & param_1 >> 4 & 0xffff) == 0)) {
    uVar4 = 0;
  }
  else {
    uVar4 = 1;
  }
  return uVar4;
}


// ==== RTC_ClearITPendingBit @ 08020a46  [RTC]

/* StdPeriph: ISR = ~((it >> 4) | INIT) | (ISR & INIT); RTC_WKUP_IRQHandler RTC_IT_WUT. library */

void RTC_ClearITPendingBit(uint param_1)

{
  dword dVar1;
  
  dVar1 = RTC.ISR;
  RTC.ISR = dVar1 & 0x80 | ~(param_1 >> 4 & 0xffff | 0x80);
  return;
}


// ==== SYSCFG_EXTILineConfig @ 08020a74  [SYSCFG]

/* StdPeriph: EXTICRx nibble = port source; Wake_ArmRotary path uses port 7 = GPIOG (rotary on PG,
   STM32L152ZD). library */

void SYSCFG_EXTILineConfig(byte param_1,byte param_2)

{
  (&SYSCFG.EXTICR1)[(int)(uint)param_2 >> 2] =
       (&SYSCFG.EXTICR1)[(int)(uint)param_2 >> 2] & ~(0xf << ((uint)(param_2 & 3) << 2));
  (&SYSCFG.EXTICR1)[(int)(uint)param_2 >> 2] =
       (&SYSCFG.EXTICR1)[(int)(uint)param_2 >> 2] | (uint)param_1 << ((param_2 & 3) << 2);
  return;
}


// ==== EXTI_Init @ 08020ac8  [EXTI]

/* StdPeriph: {Line, Mode +4, Trigger +5 (0x10 = both), LineCmd +6}; rotary wake uses 0x0C falling.
   library */

void EXTI_Init(uint *param_1)

{
  dword dVar1;
  uint *puVar2;
  
  if (*(char *)((int)param_1 + 6) == '\0') {
    puVar2 = (uint *)((int)&EXTI.IMR + (uint)(byte)param_1[1]);
    *puVar2 = *puVar2 & ~*param_1;
  }
  else {
    dVar1 = EXTI.IMR;
    EXTI.IMR = dVar1 & ~*param_1;
    dVar1 = EXTI.EMR;
    EXTI.EMR = dVar1 & ~*param_1;
    puVar2 = (uint *)((int)&EXTI.IMR + (uint)(byte)param_1[1]);
    *puVar2 = *puVar2 | *param_1;
    dVar1 = EXTI.RTSR;
    EXTI.RTSR = dVar1 & ~*param_1;
    dVar1 = EXTI.FTSR;
    EXTI.FTSR = dVar1 & ~*param_1;
    if (*(char *)((int)param_1 + 5) == '\x10') {
      dVar1 = EXTI.RTSR;
      EXTI.RTSR = dVar1 | *param_1;
      dVar1 = EXTI.FTSR;
      EXTI.FTSR = dVar1 | *param_1;
    }
    else {
      puVar2 = (uint *)((int)&EXTI.IMR + (uint)*(byte *)((int)param_1 + 5));
      *puVar2 = *puVar2 | *param_1;
    }
  }
  return;
}


// ==== EXTI_GetFlagStatus @ 08020b50  [EXTI]

/* StdPeriph: PR & line (no IMR test, so not GetITStatus); EXTI0..4 and EXTI9_5 handlers. library */

bool EXTI_GetFlagStatus(uint param_1)

{
  dword dVar1;
  
  dVar1 = EXTI.PR;
  return (dVar1 & param_1) != 0;
}


// ==== EXTI_ClearITPendingBit @ 08020b6a  [EXTI]

/* StdPeriph: PR = line (same body as EXTI_ClearFlag); EXTI handlers and RTC_WKUP (line 20); which
   of the identical pair is a guess. library */

void EXTI_ClearITPendingBit(dword param_1)

{
  EXTI.PR = param_1;
  return;
}


// ==== mem_cpy @ 08020b84

/* library (FatFs static): byte copy; dir_register, f_read, f_write. high */

void mem_cpy(undefined1 *param_1,undefined1 *param_2,int param_3)

{
  while (param_3 != 0) {
    *param_1 = *param_2;
    param_2 = param_2 + 1;
    param_1 = param_1 + 1;
    param_3 = param_3 + -1;
  }
  return;
}


// ==== mem_set @ 08020ba0

/* library (FatFs static): byte fill; sync, dir_next, dir_register, create_name. high */

void mem_set(undefined1 *param_1,undefined1 param_2,int param_3)

{
  while (param_3 != 0) {
    *param_1 = param_2;
    param_1 = param_1 + 1;
    param_3 = param_3 + -1;
  }
  return;
}


// ==== mem_cmp @ 08020bb6

/* library (FatFs static): byte compare, returns difference; dir_find 11-byte name. high */

int mem_cmp(byte *param_1,byte *param_2,int param_3)

{
  byte bVar1;
  byte bVar2;
  
  do {
    if (param_3 == 0) {
      return 0;
    }
    bVar1 = *param_1;
    bVar2 = *param_2;
    param_2 = param_2 + 1;
    param_1 = param_1 + 1;
    param_3 = param_3 + -1;
  } while ((uint)bVar1 - (uint)bVar2 == 0);
  return (uint)bVar1 - (uint)bVar2;
}


// ==== chk_chr @ 08020bda

/* library (FatFs static): strchr-style test; create_name checks illegal chars (0x08029164). high */

byte chk_chr(byte *param_1,uint param_2)

{
  for (; (*param_1 != 0 && (*param_1 != param_2)); param_1 = param_1 + 1) {
  }
  return *param_1;
}


// ==== move_window @ 08020bee

/* library (FatFs): flush dirty win (wflag) with disk_write, mirror into the other FATs (n_fats)
   when in the FAT area, then disk_read the new sector into fs->win (+0x30); 1 = FR_DISK_ERR. high
    */

undefined8 move_window(int param_1,uint param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  int iVar2;
  undefined4 uVar3;
  byte bVar4;
  
  uVar1 = *(uint *)(param_1 + 0x2c);
  if (uVar1 != param_2) {
    if (*(char *)(param_1 + 4) != '\0') {
      iVar2 = disk_write(*(undefined1 *)(param_1 + 1),param_1 + 0x30,uVar1,1);
      if (iVar2 != 0) {
        uVar3 = 1;
        goto LAB_08020c64;
      }
      *(undefined1 *)(param_1 + 4) = 0;
      if (uVar1 < (uint)(*(int *)(param_1 + 0x1c) + *(int *)(param_1 + 0x20))) {
        for (bVar4 = *(byte *)(param_1 + 3); 1 < bVar4; bVar4 = bVar4 - 1) {
          uVar1 = *(int *)(param_1 + 0x1c) + uVar1;
          disk_write(*(undefined1 *)(param_1 + 1),param_1 + 0x30,uVar1,1);
        }
      }
    }
    if (param_2 != 0) {
      iVar2 = disk_read(*(undefined1 *)(param_1 + 1),param_1 + 0x30,param_2,1);
      if (iVar2 != 0) {
        uVar3 = 1;
        goto LAB_08020c64;
      }
      *(uint *)(param_1 + 0x2c) = param_2;
    }
  }
  uVar3 = 0;
LAB_08020c64:
  return CONCAT44(param_4,uVar3);
}


// ==== sync @ 08020c66

/* library (FatFs R0.09 "sync", later sync_fs): move_window(0) flush; FAT32 with fsi_flag: rebuilds
   FSInfo (RRaA/rrAa, free_clust, last_clust, 0xAA55) and writes it; then disk_ioctl CTRL_SYNC. high
    */

undefined8 sync(char *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  int iVar2;
  
  uVar1 = move_window(param_1,0);
  uVar1 = uVar1 & 0xff;
  if (uVar1 == 0) {
    if ((*param_1 == '\x03') && (param_1[5] != '\0')) {
      param_1[0x2c] = '\0';
      param_1[0x2d] = '\0';
      param_1[0x2e] = '\0';
      param_1[0x2f] = '\0';
      mem_set(param_1 + 0x30,0,0x200);
      param_1[0x22e] = 'U';
      param_1[0x22f] = -0x56;
      param_1[0x30] = 'R';
      param_1[0x31] = 'R';
      param_1[0x32] = 'a';
      param_1[0x33] = 'A';
      param_1[0x214] = 'r';
      param_1[0x215] = 'r';
      param_1[0x216] = 'A';
      param_1[0x217] = 'a';
      param_1[0x218] = (char)*(undefined4 *)(param_1 + 0x10);
      param_1[0x219] = (char)((uint)*(undefined4 *)(param_1 + 0x10) >> 8);
      param_1[0x21a] = (char)((uint)*(undefined4 *)(param_1 + 0x10) >> 0x10);
      param_1[0x21b] = (char)((uint)*(undefined4 *)(param_1 + 0x10) >> 0x18);
      param_1[0x21c] = (char)*(undefined4 *)(param_1 + 0xc);
      param_1[0x21d] = (char)((uint)*(undefined4 *)(param_1 + 0xc) >> 8);
      param_1[0x21e] = (char)((uint)*(undefined4 *)(param_1 + 0xc) >> 0x10);
      param_1[0x21f] = (char)((uint)*(undefined4 *)(param_1 + 0xc) >> 0x18);
      disk_write(param_1[1],param_1 + 0x30,*(undefined4 *)(param_1 + 0x14),1);
      param_1[5] = '\0';
    }
    iVar2 = disk_ioctl(param_1[1],0,0);
    if (iVar2 != 0) {
      uVar1 = 1;
    }
  }
  return CONCAT44(param_4,uVar1);
}


// ==== clust2sect @ 08020d3e

/* library (FatFs): clst - 2 < n_fatent - 2 ? (clst - 2) * csize + database : 0. high */

int clust2sect(int param_1,int param_2)

{
  int iVar1;
  
  if (param_2 - 2U < *(int *)(param_1 + 0x18) - 2U) {
    iVar1 = (uint)*(byte *)(param_1 + 2) * (param_2 - 2U) + *(int *)(param_1 + 0x28);
  }
  else {
    iVar1 = 0;
  }
  return iVar1;
}


// ==== get_fat @ 08020d58

/* library (FatFs): FAT12/16/32 entry read via move_window; clst < 2 or >= n_fatent returns 1, disk
   error 0xFFFFFFFF; FAT32 masks 0x0FFFFFFF. high */

uint get_fat(byte *param_1,uint param_2)

{
  byte bVar1;
  int iVar2;
  uint uVar3;
  
  if ((param_2 < 2) || (*(uint *)(param_1 + 0x18) <= param_2)) {
    return 1;
  }
  bVar1 = *param_1;
  if (bVar1 == 1) {
    uVar3 = param_2 + (param_2 >> 1);
    iVar2 = move_window(param_1,*(int *)(param_1 + 0x20) + (uVar3 >> 9));
    if (iVar2 == 0) {
      bVar1 = param_1[(uVar3 & 0x1ff) + 0x30];
      iVar2 = move_window(param_1,*(int *)(param_1 + 0x20) + (uVar3 + 1 >> 9));
      if (iVar2 == 0) {
        if ((int)(param_2 << 0x1f) < 0) {
          return (uint)(ushort)(CONCAT11(param_1[(uVar3 + 1 & 0x1ff) + 0x30],bVar1) >> 4);
        }
        return CONCAT11(param_1[(uVar3 + 1 & 0x1ff) + 0x30],bVar1) & 0xfff;
      }
    }
  }
  else if (bVar1 != 0) {
    if (bVar1 == 3) {
      iVar2 = move_window(param_1,*(int *)(param_1 + 0x20) + (param_2 >> 7));
      if (iVar2 == 0) {
        param_2 = param_2 & 0x7f;
        return (uint)param_1[param_2 * 4 + 0x32] << 0x10 |
               (param_1[param_2 * 4 + 0x33] & 0xf) << 0x18 | (uint)param_1[param_2 * 4 + 0x31] << 8
               | (uint)param_1[param_2 * 4 + 0x30];
      }
    }
    else if ((bVar1 < 3) &&
            (iVar2 = move_window(param_1,*(int *)(param_1 + 0x20) + (param_2 >> 8)), iVar2 == 0)) {
      return (uint)*(ushort *)(param_1 + (param_2 & 0xff) * 2 + 0x30);
    }
  }
  return 0xffffffff;
}


// ==== put_fat @ 08020e3a

/* library (FatFs): FAT12 nibble split, FAT16 word, FAT32 keeps top 4 bits; sets fs->wflag;
   FR_INT_ERR (2) on bad cluster. high */

undefined8 put_fat(byte *param_1,uint param_2,uint param_3,undefined4 param_4)

{
  byte bVar1;
  byte bVar2;
  uint uVar3;
  uint uVar4;
  
  if ((param_2 < 2) || (*(uint *)(param_1 + 0x18) <= param_2)) {
    uVar4 = 2;
    goto LAB_08020f74;
  }
  bVar2 = *param_1;
  bVar1 = (byte)(param_3 >> 8);
  if (bVar2 == 1) {
    uVar3 = param_2 + (param_2 >> 1);
    uVar4 = move_window(param_1,*(int *)(param_1 + 0x20) + (uVar3 >> 9));
    uVar4 = uVar4 & 0xff;
    if (uVar4 == 0) {
      if ((int)(param_2 << 0x1f) < 0) {
        uVar4 = param_1[(uVar3 & 0x1ff) + 0x30] & 0xf | param_3 << 4;
      }
      else {
        uVar4 = param_3;
      }
      param_1[(uVar3 & 0x1ff) + 0x30] = (byte)uVar4;
      param_1[4] = 1;
      uVar4 = move_window(param_1,*(int *)(param_1 + 0x20) + (uVar3 + 1 >> 9));
      uVar4 = uVar4 & 0xff;
      if (uVar4 == 0) {
        if ((int)(param_2 << 0x1f) < 0) {
          bVar2 = (byte)(param_3 >> 4);
        }
        else {
          bVar2 = param_1[(uVar3 + 1 & 0x1ff) + 0x30] & 0xf0 | bVar1 & 0xf;
        }
        param_1[(uVar3 + 1 & 0x1ff) + 0x30] = bVar2;
      }
    }
  }
  else if (bVar2 == 0) {
LAB_08020f6c:
    uVar4 = 2;
  }
  else if (bVar2 == 3) {
    uVar4 = move_window(param_1,*(int *)(param_1 + 0x20) + (param_2 >> 7));
    uVar4 = uVar4 & 0xff;
    if (uVar4 == 0) {
      param_2 = param_2 & 0x7f;
      param_1[param_2 * 4 + 0x30] = (byte)param_3;
      param_1[param_2 * 4 + 0x31] = bVar1;
      param_1[param_2 * 4 + 0x32] = (byte)(param_3 >> 0x10);
      param_1[param_2 * 4 + 0x33] = (byte)(param_3 >> 0x18) | param_1[param_2 * 4 + 0x33] & 0xf0;
    }
  }
  else {
    if (2 < bVar2) goto LAB_08020f6c;
    uVar4 = move_window(param_1,*(int *)(param_1 + 0x20) + (param_2 >> 8));
    uVar4 = uVar4 & 0xff;
    if (uVar4 == 0) {
      param_1[(param_2 & 0xff) * 2 + 0x30] = (byte)param_3;
      param_1[(param_2 & 0xff) * 2 + 0x31] = bVar1;
    }
  }
  param_1[4] = 1;
LAB_08020f74:
  return CONCAT44(param_4,uVar4);
}


// ==== remove_chain @ 08020f7c

/* library (FatFs): frees a cluster chain with put_fat(0), free_clust++ and fsi_flag when known;
   FR_INT_ERR 2 on bad cluster / broken chain; called by f_open FA_CREATE_ALWAYS. high */

undefined8 remove_chain(int param_1,uint param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  uint uVar2;
  
  if ((param_2 < 2) || (*(uint *)(param_1 + 0x18) <= param_2)) {
    uVar2 = 2;
  }
  else {
    uVar2 = 0;
    while( true ) {
      if ((*(uint *)(param_1 + 0x18) <= param_2) || (uVar1 = get_fat(param_1,param_2), uVar1 == 0))
      goto LAB_08020fe8;
      if (uVar1 == 1) break;
      if (uVar1 == 0xffffffff) {
        uVar2 = 1;
        goto LAB_08020fe8;
      }
      uVar2 = put_fat(param_1,param_2,0);
      uVar2 = uVar2 & 0xff;
      if (uVar2 != 0) goto LAB_08020fe8;
      if (*(int *)(param_1 + 0x10) != -1) {
        *(int *)(param_1 + 0x10) = *(int *)(param_1 + 0x10) + 1;
        *(undefined1 *)(param_1 + 5) = 1;
      }
      param_2 = uVar1;
    }
    uVar2 = 2;
  }
LAB_08020fe8:
  return CONCAT44(param_4,uVar2);
}


// ==== create_chain @ 08020fee

/* library (FatFs): clst 0 = new chain from last_clust hint, else stretch; scans for a free entry,
   marks 0x0FFFFFFF and links; returns new cluster, 0 disk full, 1 internal error, 0xFFFFFFFF disk
   error; updates last_clust/free_clust. high */

undefined8 create_chain(int param_1,uint param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  
  if (param_2 == 0) {
    uVar2 = *(uint *)(param_1 + 0xc);
    if ((uVar2 == 0) || (*(uint *)(param_1 + 0x18) <= uVar2)) {
      uVar2 = 1;
    }
  }
  else {
    uVar3 = get_fat(param_1,param_2);
    if (uVar3 < 2) {
      uVar3 = 1;
      goto LAB_080210c6;
    }
    if (uVar3 < *(uint *)(param_1 + 0x18)) goto LAB_080210c6;
    uVar2 = param_2;
  }
  uVar4 = uVar2;
  do {
    uVar4 = uVar4 + 1;
    if ((*(uint *)(param_1 + 0x18) <= uVar4) && (uVar4 = 2, uVar2 < 2)) {
      uVar3 = 0;
      goto LAB_080210c6;
    }
    uVar3 = get_fat(param_1,uVar4);
    if (uVar3 == 0) {
      cVar1 = put_fat(param_1,uVar4,0xfffffff);
      if ((cVar1 == '\0') && (param_2 != 0)) {
        cVar1 = put_fat(param_1,param_2,uVar4);
      }
      if (cVar1 == '\0') {
        *(uint *)(param_1 + 0xc) = uVar4;
        uVar3 = uVar4;
        if (*(int *)(param_1 + 0x10) != -1) {
          *(int *)(param_1 + 0x10) = *(int *)(param_1 + 0x10) + -1;
          *(undefined1 *)(param_1 + 5) = 1;
        }
      }
      else if (cVar1 == '\x01') {
        uVar3 = 0xffffffff;
      }
      else {
        uVar3 = 1;
      }
      goto LAB_080210c6;
    }
    if ((uVar3 == 0xffffffff) || (uVar3 == 1)) goto LAB_080210c6;
  } while (uVar4 != uVar2);
  uVar3 = 0;
LAB_080210c6:
  return CONCAT44(param_4,uVar3);
}


// ==== dir_sdi @ 080210ca

/* library (FatFs R0.09; later dir_seek): set DIR index, static root (FAT12/16) bounded by
   n_rootdir, else walk the chain 16 entries/sector x csize; fills clust/sect/dir pointer. high */

undefined8 dir_sdi(int *param_1,ushort param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  uint uVar2;
  undefined4 uVar3;
  int iVar4;
  
  *(ushort *)((int)param_1 + 6) = param_2;
  uVar2 = param_1[2];
  if ((uVar2 == 1) || (*(uint *)(*param_1 + 0x18) <= uVar2)) {
    uVar3 = 2;
  }
  else {
    if ((uVar2 == 0) && (*(char *)*param_1 == '\x03')) {
      uVar2 = *(uint *)(*param_1 + 0x24);
    }
    if (uVar2 == 0) {
      param_1[3] = 0;
      if (*(ushort *)(*param_1 + 8) <= param_2) {
        uVar3 = 2;
        goto LAB_0802117e;
      }
      param_1[4] = *(int *)(*param_1 + 0x24) + (uint)(param_2 >> 4);
    }
    else {
      bVar1 = *(byte *)(*param_1 + 2);
      for (; (ushort)((ushort)bVar1 * 0x10) <= param_2; param_2 = param_2 + (ushort)bVar1 * -0x10) {
        uVar2 = get_fat(*param_1,uVar2);
        if (uVar2 == 0xffffffff) {
          uVar3 = 1;
          goto LAB_0802117e;
        }
        if ((uVar2 < 2) || (*(uint *)(*param_1 + 0x18) <= uVar2)) {
          uVar3 = 2;
          goto LAB_0802117e;
        }
      }
      param_1[3] = uVar2;
      iVar4 = clust2sect(*param_1,uVar2);
      param_1[4] = iVar4 + (uint)(param_2 >> 4);
    }
    param_1[5] = *param_1 + (param_2 & 0xf) * 0x20 + 0x30;
    uVar3 = 0;
  }
LAB_0802117e:
  return CONCAT44(param_4,uVar3);
}


// ==== dir_next @ 08021180

/* library (FatFs): advance DIR index; static root ends at n_rootdir, else next cluster via get_fat;
   stretch=1 extends with create_chain and zero-fills the new cluster (FR_DENIED 7 when full,
   FR_NO_FILE 4 at end). high */

undefined4 dir_next(int *param_1,int param_2)

{
  ushort uVar1;
  uint uVar2;
  int iVar3;
  undefined4 uVar4;
  uint uVar5;
  byte bVar6;
  
  uVar1 = *(short *)((int)param_1 + 6) + 1;
  uVar5 = (uint)uVar1;
  if ((uVar5 == 0) || (param_1[4] == 0)) {
    return 4;
  }
  if ((uVar1 & 0xf) == 0) {
    param_1[4] = param_1[4] + 1;
    if (param_1[3] == 0) {
      if (*(ushort *)(*param_1 + 8) <= uVar5) {
        return 4;
      }
    }
    else if ((ushort)(*(byte *)(*param_1 + 2) - 1 & uVar1 >> 4) == 0) {
      uVar2 = get_fat(*param_1,param_1[3]);
      if (uVar2 < 2) {
        return 2;
      }
      if (uVar2 == 0xffffffff) {
        return 1;
      }
      if (*(uint *)(*param_1 + 0x18) <= uVar2) {
        if (param_2 == 0) {
          return 4;
        }
        uVar2 = create_chain(*param_1,param_1[3]);
        if (uVar2 == 0) {
          return 7;
        }
        if (uVar2 == 1) {
          return 2;
        }
        if (uVar2 == 0xffffffff) {
          return 1;
        }
        iVar3 = move_window(*param_1,0);
        if (iVar3 != 0) {
          return 1;
        }
        mem_set(*param_1 + 0x30,0,0x200);
        uVar4 = clust2sect(*param_1,uVar2);
        *(undefined4 *)(*param_1 + 0x2c) = uVar4;
        for (bVar6 = 0; bVar6 < *(byte *)(*param_1 + 2); bVar6 = bVar6 + 1) {
          *(undefined1 *)(*param_1 + 4) = 1;
          iVar3 = move_window(*param_1,0);
          if (iVar3 != 0) {
            return 1;
          }
          *(int *)(*param_1 + 0x2c) = *(int *)(*param_1 + 0x2c) + 1;
        }
        *(uint *)(*param_1 + 0x2c) = *(int *)(*param_1 + 0x2c) - (uint)bVar6;
      }
      param_1[3] = uVar2;
      iVar3 = clust2sect(*param_1,uVar2);
      param_1[4] = iVar3;
    }
  }
  *(ushort *)((int)param_1 + 6) = uVar1;
  param_1[5] = *param_1 + (uVar5 & 0xf) * 0x20 + 0x30;
  return 0;
}


// ==== dir_find @ 080212b2

/* library (FatFs, SFN only): dir_sdi(0), move_window per entry, compare 11-byte name (mem_cmp) on
   non-volume-label entries; 4 = FR_NO_FILE at end. high */

undefined8 dir_find(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  uint uVar2;
  char *pcVar3;
  int iVar4;
  
  bVar1 = dir_sdi(param_1,0);
  if (bVar1 == 0) {
    do {
      uVar2 = move_window(*param_1,param_1[4]);
      uVar2 = uVar2 & 0xff;
      if (uVar2 != 0) break;
      pcVar3 = (char *)param_1[5];
      if (*pcVar3 == '\0') {
        uVar2 = 4;
        break;
      }
      if ((-1 < (int)((uint)(byte)pcVar3[0xb] << 0x1c)) &&
         (iVar4 = mem_cmp(pcVar3,param_1[6],0xb), iVar4 == 0)) break;
      uVar2 = dir_next(param_1,0);
      uVar2 = uVar2 & 0xff;
    } while (uVar2 == 0);
  }
  else {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== dir_read @ 0802131a

/* library (FatFs, SFN, _FS_RPATH 0): next live entry, skipping 0xE5, "." entries and volume labels;
   4 at end, clears dj->sect. high */

undefined8 dir_read(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  char cVar1;
  uint uVar2;
  
  uVar2 = 4;
  do {
    if (param_1[4] == 0) break;
    uVar2 = move_window(*param_1,param_1[4]);
    uVar2 = uVar2 & 0xff;
    if (uVar2 != 0) break;
    cVar1 = *(char *)param_1[5];
    if (cVar1 == '\0') {
      uVar2 = 4;
      break;
    }
    if (((cVar1 != -0x1b) && (cVar1 != '.')) &&
       (-1 < (int)((uint)(byte)((char *)param_1[5])[0xb] << 0x1c))) break;
    uVar2 = dir_next(param_1,0);
    uVar2 = uVar2 & 0xff;
  } while (uVar2 == 0);
  if (uVar2 != 0) {
    param_1[4] = 0;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== dir_register @ 0802137e

/* library (FatFs, SFN): finds a free (0x00/0xE5) slot, stretching the directory via dir_next(1),
   clears 32 bytes, copies the 11-byte name, sets wflag. high */

undefined8 dir_register(int *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  int iVar2;
  
  uVar1 = dir_sdi(param_1,0);
  while (uVar1 = uVar1 & 0xff, uVar1 == 0) {
    uVar1 = move_window(*param_1,param_1[4]);
    uVar1 = uVar1 & 0xff;
    if (((uVar1 != 0) || (*(char *)param_1[5] == -0x1b)) || (*(char *)param_1[5] == '\0')) break;
    uVar1 = dir_next(param_1,1);
  }
  if (uVar1 == 0) {
    uVar1 = move_window(*param_1,param_1[4]);
    uVar1 = uVar1 & 0xff;
    if (uVar1 == 0) {
      iVar2 = param_1[5];
      mem_set(iVar2,0,0x20);
      mem_cpy(iVar2,param_1[6],0xb);
      *(undefined1 *)(*param_1 + 4) = 1;
    }
  }
  return CONCAT44(param_4,uVar1);
}


// ==== create_name @ 08021400

/* library (FatFs): SFN-only (_USE_LFN 0) 8.3 builder, upper-cases, illegal-char table 08029164,
   DBCS lead/trail ranges 0x81-0x9F/0xE0-0xFC = code page 932 (ffconf default), 0xE5 -> 0x05,
   NS_BODY/NS_EXT case flags in byte 11, FR_INVALID_NAME 6. high */

undefined8 create_name(int param_1,int *param_2)

{
  byte bVar1;
  char *pcVar2;
  int iVar3;
  int iVar4;
  undefined4 uVar5;
  byte bVar6;
  byte bVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  
  pcVar2 = *(char **)(param_1 + 0x18);
  mem_set(pcVar2,0x20,0xb);
  bVar6 = 0;
  uVar10 = 0;
  uVar8 = 8;
  iVar3 = *param_2;
  iVar4 = 0;
LAB_0802142a:
  do {
    bVar7 = *(byte *)(iVar3 + iVar4);
    iVar9 = iVar4 + 1;
    if (((bVar7 < 0x21) || (bVar7 == 0x2f)) || (bVar7 == 0x5c)) {
      *param_2 = iVar3 + iVar9;
      if (bVar7 < 0x21) {
        bVar7 = 4;
      }
      else {
        bVar7 = 0;
      }
      if (uVar10 == 0) {
        uVar5 = 6;
      }
      else {
        if (*pcVar2 == -0x1b) {
          *pcVar2 = '\x05';
        }
        if (uVar8 == 8) {
          bVar6 = bVar6 << 2;
        }
        if ((bVar6 & 3) == 1) {
          bVar7 = bVar7 | 0x10;
        }
        if ((bVar6 & 0xc) == 4) {
          bVar7 = bVar7 | 8;
        }
        pcVar2[0xb] = bVar7;
        uVar5 = 0;
      }
LAB_0802156a:
      return CONCAT44(iVar3,uVar5);
    }
    if ((bVar7 == 0x2e) || (uVar8 <= uVar10)) {
      if ((uVar8 != 8) || (bVar7 != 0x2e)) {
        uVar5 = 6;
        goto LAB_0802156a;
      }
      uVar10 = 8;
      uVar8 = 0xb;
      bVar6 = bVar6 << 2;
      iVar4 = iVar9;
      goto LAB_0802142a;
    }
    if (0x7f < bVar7) {
      bVar6 = bVar6 | 3;
    }
    if (((bVar7 < 0x81) || (0x9f < bVar7)) && ((bVar7 < 0xe0 || (0xfc < bVar7)))) {
      iVar4 = chk_chr(&DAT_08029164,bVar7);
      if (iVar4 != 0) {
        uVar5 = 6;
        goto LAB_0802156a;
      }
      if ((bVar7 < 0x41) || (0x5a < bVar7)) {
        if ((0x60 < bVar7) && (bVar7 < 0x7b)) {
          bVar6 = bVar6 | 1;
          bVar7 = bVar7 - 0x20;
        }
      }
      else {
        bVar6 = bVar6 | 2;
      }
      pcVar2[uVar10] = bVar7;
      uVar10 = uVar10 + 1;
      iVar4 = iVar9;
    }
    else {
      bVar1 = *(byte *)(iVar3 + iVar9);
      if ((((bVar1 < 0x40) || (0x7e < bVar1)) && ((bVar1 < 0x80 || (0xfc < bVar1)))) ||
         (uVar8 - 1 <= uVar10)) {
        uVar5 = 6;
        goto LAB_0802156a;
      }
      pcVar2[uVar10] = bVar7;
      pcVar2[uVar10 + 1] = bVar1;
      uVar10 = uVar10 + 2;
      iVar4 = iVar4 + 2;
    }
  } while( true );
}


// ==== get_fileinfo @ 0802156e

/* library (FatFs): FILINFO from the current entry: "NAME.EXT" (0x05 -> 0xE5), attr, fsize, fdate
   (+0x18), ftime (+0x16); empty name when dj->sect 0. high */

void get_fileinfo(int param_1,uint *param_2)

{
  char cVar1;
  uint uVar2;
  char *pcVar3;
  int iVar4;
  
  pcVar3 = (char *)((int)param_2 + 9);
  if (*(int *)(param_1 + 0x10) != 0) {
    iVar4 = *(int *)(param_1 + 0x14);
    for (uVar2 = 0; (uVar2 < 8 && (cVar1 = *(char *)(iVar4 + uVar2), cVar1 != ' '));
        uVar2 = uVar2 + 1) {
      if (cVar1 == '\x05') {
        cVar1 = -0x1b;
      }
      *pcVar3 = cVar1;
      pcVar3 = pcVar3 + 1;
    }
    if (*(char *)(iVar4 + 8) != ' ') {
      *pcVar3 = '.';
      for (uVar2 = 8; (pcVar3 = pcVar3 + 1, uVar2 < 0xb && (*(char *)(iVar4 + uVar2) != ' '));
          uVar2 = uVar2 + 1) {
        *pcVar3 = *(char *)(iVar4 + uVar2);
      }
    }
    *(undefined1 *)(param_2 + 2) = *(undefined1 *)(iVar4 + 0xb);
    *param_2 = (uint)*(byte *)(iVar4 + 0x1c) |
               (uint)*(byte *)(iVar4 + 0x1e) << 0x10 | (uint)*(byte *)(iVar4 + 0x1f) << 0x18 |
               (uint)*(byte *)(iVar4 + 0x1d) << 8;
    *(undefined2 *)(param_2 + 1) = *(undefined2 *)(iVar4 + 0x18);
    *(undefined2 *)((int)param_2 + 6) = *(undefined2 *)(iVar4 + 0x16);
  }
  *pcVar3 = '\0';
  return;
}


// ==== follow_path @ 08021612

/* library (FatFs): skips leading '/' or '\', create_name + dir_find per segment, descends into
   directories (sclust from 0x14/0x1A); 5 = FR_NO_PATH; empty path = root (dj->dir NULL). high */

ulonglong follow_path(int param_1,byte *param_2)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  byte *local_18;
  
  if ((*param_2 == 0x2f) || (local_18 = param_2, *param_2 == 0x5c)) {
    local_18 = param_2 + 1;
  }
  *(undefined4 *)(param_1 + 8) = 0;
  if (*local_18 < 0x20) {
    uVar1 = dir_sdi(param_1,0);
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  else {
    while( true ) {
      uVar1 = create_name(param_1,&local_18);
      uVar1 = uVar1 & 0xff;
      if (uVar1 != 0) break;
      uVar1 = dir_find(param_1);
      uVar2 = (uint)*(byte *)(*(int *)(param_1 + 0x18) + 0xb);
      if ((uVar1 & 0xff) != 0) {
        uVar1 = uVar1 & 0xff;
        if ((uVar1 == 4) && (-1 < (int)(uVar2 << 0x1d))) {
          uVar1 = 5;
        }
        break;
      }
      uVar1 = 0;
      if ((int)(uVar2 << 0x1d) < 0) break;
      iVar3 = *(int *)(param_1 + 0x14);
      if (-1 < (int)((uint)*(byte *)(iVar3 + 0xb) << 0x1b)) {
        uVar1 = 5;
        break;
      }
      *(uint *)(param_1 + 8) = CONCAT22(*(undefined2 *)(iVar3 + 0x14),*(undefined2 *)(iVar3 + 0x1a))
      ;
    }
  }
  return CONCAT44(local_18,uVar1) & 0xffffffff000000ff;
}


// ==== check_fs @ 080216b2

/* library (FatFs): reads the sector, 0xAA55 at 510 else 2, "FAT" at 0x36 (FAT12/16) or 0x52 (FAT32)
   = 0, else 1; 3 on disk error; called twice by chk_mounted (sector 0, then the MBR partition).
   high */

undefined8 check_fs(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = disk_read(*(undefined1 *)(param_1 + 1),param_1 + 0x30,param_2,1);
  if (iVar1 == 0) {
    if (*(short *)(param_1 + 0x22e) == -0x55ab) {
      if (((uint)*(byte *)(param_1 + 0x67) << 8 | (uint)*(byte *)(param_1 + 0x68) << 0x10 |
          (uint)*(byte *)(param_1 + 0x66)) == 0x544146) {
        uVar2 = 0;
      }
      else if (((uint)*(byte *)(param_1 + 0x83) << 8 | (uint)*(byte *)(param_1 + 0x84) << 0x10 |
               (uint)*(byte *)(param_1 + 0x82)) == 0x544146) {
        uVar2 = 0;
      }
      else {
        uVar2 = 1;
      }
    }
    else {
      uVar2 = 2;
    }
  }
  else {
    uVar2 = 3;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== chk_mounted @ 0802172c

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* library (FatFs R0.09x; became find_volume in R0.10): "N:" drive prefix, FR_INVALID_DRIVE 11 /
   FR_NOT_ENABLED 12 / FR_NOT_READY 3 / FR_WRITE_PROTECTED 10, check_fs at 0 then MBR partition 1
   only, 512-B sector, FAT12/16/32 by cluster count (0xFF5/0xFFF5), FAT32 FSInfo read by disk_read
   directly (0x41615252 / 0x61417272 / 0xAA55); FATFS layout has no volbase. high (emulated) */

undefined4 chk_mounted(undefined4 *param_1,undefined4 *param_2,char param_3)

{
  byte bVar1;
  ushort uVar2;
  char *pcVar3;
  byte bVar4;
  char cVar5;
  undefined4 uVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  int iVar10;
  byte *pbVar11;
  uint local_44;
  uint local_40;
  uint local_34;
  
  pbVar11 = (byte *)*param_1;
  local_40 = *pbVar11 - 0x30;
  if ((local_40 < 10) && (pbVar11[1] == 0x3a)) {
    *param_1 = pbVar11 + 2;
  }
  else {
    local_40 = 0;
  }
  pcVar3 = _DAT_20007c58;
  if (local_40 == 0) {
    *param_2 = _DAT_20007c58;
    if (pcVar3 == (char *)0x0) {
      uVar6 = 0xc;
    }
    else {
      if ((*pcVar3 != '\0') && (bVar4 = disk_status(pcVar3[1]), -1 < (int)((uint)bVar4 << 0x1f))) {
        if ((param_3 == '\0') || (-1 < (int)((uint)bVar4 << 0x1d))) {
          return 0;
        }
        return 10;
      }
      *pcVar3 = '\0';
      pcVar3[1] = '\0';
      bVar4 = disk_initialize(pcVar3[1]);
      if ((int)((uint)bVar4 << 0x1f) < 0) {
        uVar6 = 3;
      }
      else if ((param_3 == '\0') || (-1 < (int)((uint)bVar4 << 0x1d))) {
        local_44 = 0;
        cVar5 = check_fs(pcVar3,0);
        if ((cVar5 == '\x01') && (pcVar3[0x1f2] != '\0')) {
          local_44 = (uint)(byte)pcVar3[0x1f8] << 0x10 | (uint)(byte)pcVar3[0x1f9] << 0x18 |
                     (uint)(byte)pcVar3[0x1f7] << 8 | (uint)(byte)pcVar3[0x1f6];
          cVar5 = check_fs(pcVar3,local_44);
        }
        if (cVar5 == '\x03') {
          uVar6 = 1;
        }
        else if (cVar5 == '\0') {
          if (*(short *)(pcVar3 + 0x3b) == 0x200) {
            uVar7 = (uint)*(ushort *)(pcVar3 + 0x46);
            if (uVar7 == 0) {
              uVar7 = (uint)(byte)pcVar3[0x56] << 0x10 | (uint)(byte)pcVar3[0x57] << 0x18 |
                      (uint)(byte)pcVar3[0x55] << 8 | (uint)(byte)pcVar3[0x54];
            }
            *(uint *)(pcVar3 + 0x1c) = uVar7;
            bVar4 = pcVar3[0x40];
            pcVar3[3] = bVar4;
            if ((bVar4 == 1) || (bVar4 == 2)) {
              bVar1 = pcVar3[0x3d];
              pcVar3[2] = bVar1;
              if ((bVar1 == 0) || ((bVar1 & bVar1 - 1) != 0)) {
                uVar6 = 0xd;
              }
              else {
                *(undefined2 *)(pcVar3 + 8) = *(undefined2 *)(pcVar3 + 0x41);
                if ((pcVar3[8] & 0xfU) == 0) {
                  local_34 = (uint)*(ushort *)(pcVar3 + 0x43);
                  if (local_34 == 0) {
                    local_34 = (uint)(byte)pcVar3[0x52] << 0x10 | (uint)(byte)pcVar3[0x53] << 0x18 |
                               (uint)(byte)pcVar3[0x51] << 8 | (uint)(byte)pcVar3[0x50];
                  }
                  uVar2 = *(ushort *)(pcVar3 + 0x3e);
                  if (uVar2 == 0) {
                    uVar6 = 0xd;
                  }
                  else {
                    uVar8 = bVar4 * uVar7 + (uint)uVar2 + (uint)(*(ushort *)(pcVar3 + 8) >> 4);
                    if (local_34 < uVar8) {
                      uVar6 = 0xd;
                    }
                    else {
                      uVar9 = (local_34 - uVar8) / (uint)(byte)pcVar3[2];
                      if (uVar9 == 0) {
                        uVar6 = 0xd;
                      }
                      else {
                        cVar5 = '\x01';
                        if (0xff5 < uVar9) {
                          cVar5 = '\x02';
                        }
                        if (0xfff5 < uVar9) {
                          cVar5 = '\x03';
                        }
                        *(uint *)(pcVar3 + 0x18) = uVar9 + 2;
                        *(uint *)(pcVar3 + 0x28) = uVar8 + local_44;
                        *(uint *)(pcVar3 + 0x20) = uVar2 + local_44;
                        if (cVar5 == '\x03') {
                          if (*(short *)(pcVar3 + 8) != 0) {
                            return 0xd;
                          }
                          *(uint *)(pcVar3 + 0x24) =
                               (uint)(byte)pcVar3[0x5e] << 0x10 | (uint)(byte)pcVar3[0x5f] << 0x18 |
                               (uint)(byte)pcVar3[0x5d] << 8 | (uint)(byte)pcVar3[0x5c];
                          iVar10 = *(int *)(pcVar3 + 0x18) << 2;
                        }
                        else {
                          if (*(short *)(pcVar3 + 8) == 0) {
                            return 0xd;
                          }
                          *(uint *)(pcVar3 + 0x24) = bVar4 * uVar7 + *(int *)(pcVar3 + 0x20);
                          if (cVar5 == '\x02') {
                            iVar10 = *(int *)(pcVar3 + 0x18) << 1;
                          }
                          else {
                            iVar10 = ((byte)pcVar3[0x18] & 1) +
                                     ((uint)(*(int *)(pcVar3 + 0x18) * 3) >> 1);
                          }
                        }
                        if (*(uint *)(pcVar3 + 0x1c) < iVar10 + 0x1ffU >> 9) {
                          uVar6 = 0xd;
                        }
                        else {
                          pcVar3[0x10] = -1;
                          pcVar3[0x11] = -1;
                          pcVar3[0x12] = -1;
                          pcVar3[0x13] = -1;
                          pcVar3[0xc] = '\0';
                          pcVar3[0xd] = '\0';
                          pcVar3[0xe] = '\0';
                          pcVar3[0xf] = '\0';
                          if (cVar5 == '\x03') {
                            pcVar3[5] = '\0';
                            *(uint *)(pcVar3 + 0x14) = *(ushort *)(pcVar3 + 0x60) + local_44;
                            iVar10 = disk_read(pcVar3[1],pcVar3 + 0x30,
                                               *(undefined4 *)(pcVar3 + 0x14),1);
                            if ((((iVar10 == 0) && (*(short *)(pcVar3 + 0x22e) == -0x55ab)) &&
                                (((uint)(byte)pcVar3[0x32] << 0x10 |
                                  (uint)(byte)pcVar3[0x33] << 0x18 | (uint)(byte)pcVar3[0x31] << 8 |
                                 (uint)(byte)pcVar3[0x30]) == 0x41615252)) &&
                               (((uint)(byte)pcVar3[0x216] << 0x10 |
                                 (uint)(byte)pcVar3[0x217] << 0x18 | (uint)(byte)pcVar3[0x215] << 8
                                | (uint)(byte)pcVar3[0x214]) == 0x61417272)) {
                              *(uint *)(pcVar3 + 0xc) =
                                   (uint)(byte)pcVar3[0x21e] << 0x10 |
                                   (uint)(byte)pcVar3[0x21f] << 0x18 |
                                   (uint)(byte)pcVar3[0x21d] << 8 | (uint)(byte)pcVar3[0x21c];
                              *(uint *)(pcVar3 + 0x10) =
                                   (uint)(byte)pcVar3[0x21a] << 0x10 |
                                   (uint)(byte)pcVar3[0x21b] << 0x18 |
                                   (uint)(byte)pcVar3[0x219] << 8 | (uint)(byte)pcVar3[0x218];
                            }
                          }
                          *pcVar3 = cVar5;
                          DAT_20007f98 = DAT_20007f98 + 1;
                          *(short *)(pcVar3 + 6) = DAT_20007f98;
                          pcVar3[0x2c] = '\0';
                          pcVar3[0x2d] = '\0';
                          pcVar3[0x2e] = '\0';
                          pcVar3[0x2f] = '\0';
                          pcVar3[4] = '\0';
                          uVar6 = 0;
                        }
                      }
                    }
                  }
                }
                else {
                  uVar6 = 0xd;
                }
              }
            }
            else {
              uVar6 = 0xd;
            }
          }
          else {
            uVar6 = 0xd;
          }
        }
        else {
          uVar6 = 0xd;
        }
      }
      else {
        uVar6 = 10;
      }
    }
  }
  else {
    uVar6 = 0xb;
  }
  return uVar6;
}


// ==== validate @ 08021b50

/* library (FatFs R0.09 static): fs NULL / fs_type 0 / id mismatch -> FR_INVALID_OBJECT 9;
   disk_status STA_NOINIT -> FR_NOT_READY 3. high */

undefined8 validate(char *param_1,short param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  int iVar2;
  
  if (((param_1 == (char *)0x0) || (*param_1 == '\0')) || (*(short *)(param_1 + 6) != param_2)) {
    uVar1 = 9;
  }
  else {
    iVar2 = disk_status(param_1[1]);
    if (iVar2 << 0x1f < 0) {
      uVar1 = 3;
    }
    else {
      uVar1 = 0;
    }
  }
  return CONCAT44(param_4,uVar1);
}


// ==== f_mount @ 08021b80

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* library (FatFs R0.09): vol != 0 -> FR_INVALID_DRIVE 11; old and new fs_type = 0, FatFs[0]
   (0x20007c58) = fs; always FR_OK for drive 0. high */

undefined4 f_mount(char param_1,undefined1 *param_2)

{
  undefined4 uVar1;
  
  if (param_1 == '\0') {
    if (_DAT_20007c58 != (undefined1 *)0x0) {
      *_DAT_20007c58 = 0;
    }
    if (param_2 != (undefined1 *)0x0) {
      *param_2 = 0;
    }
    uVar1 = 0;
    _DAT_20007c58 = param_2;
  }
  else {
    uVar1 = 0xb;
  }
  return uVar1;
}


// ==== f_open @ 08021bcc

/* library (FatFs): chk_mounted + follow_path, create via dir_register (0802137e), FA_CREATE_ALWAYS
   truncates by remove_chain (08020f7c), DIR_CrtTime from get_fattime, FIL filled (no file lock /
   _FS_SHARE 0). high (emulated) */

char f_open(int *param_1,undefined4 param_2,uint param_3)

{
  char cVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  byte bVar5;
  uint uVar6;
  int local_48 [5];
  int local_34;
  undefined1 *local_30;
  undefined1 auStack_2c [12];
  undefined4 local_20;
  
  *param_1 = 0;
  uVar6 = param_3 & 0x1f;
  local_20 = param_2;
  cVar1 = chk_mounted(&local_20,local_48,param_3 & 0x1e);
  local_30 = auStack_2c;
  if (cVar1 == '\0') {
    cVar1 = follow_path(local_48,local_20);
  }
  if ((cVar1 == '\0') && (local_34 == 0)) {
    cVar1 = '\x06';
  }
  if ((param_3 & 0x1c) == 0) {
    if (cVar1 == '\0') {
      if ((int)((uint)*(byte *)(local_34 + 0xb) << 0x1b) < 0) {
        cVar1 = '\x04';
      }
      else if (((int)(param_3 << 0x1e) < 0) && ((int)((uint)*(byte *)(local_34 + 0xb) << 0x1f) < 0))
      {
        cVar1 = '\a';
      }
    }
  }
  else {
    if (cVar1 == '\0') {
      iVar4 = local_34;
      if ((int)(param_3 << 0x1d) < 0) {
        cVar1 = '\b';
      }
      else if ((*(byte *)(local_34 + 0xb) & 0x11) != 0) {
        cVar1 = '\a';
      }
    }
    else {
      if (cVar1 == '\x04') {
        cVar1 = dir_register(local_48);
      }
      uVar6 = uVar6 | 8;
      iVar4 = local_34;
    }
    local_34 = iVar4;
    if ((cVar1 == '\0') && ((int)(uVar6 << 0x1c) < 0)) {
      uVar2 = get_fattime();
      *(char *)(iVar4 + 0xe) = (char)uVar2;
      *(char *)(iVar4 + 0xf) = (char)((uint)uVar2 >> 8);
      *(char *)(iVar4 + 0x10) = (char)((uint)uVar2 >> 0x10);
      *(char *)(iVar4 + 0x11) = (char)((uint)uVar2 >> 0x18);
      *(undefined1 *)(iVar4 + 0xb) = 0;
      *(undefined1 *)(iVar4 + 0x1c) = 0;
      *(undefined1 *)(iVar4 + 0x1d) = 0;
      *(undefined1 *)(iVar4 + 0x1e) = 0;
      *(undefined1 *)(iVar4 + 0x1f) = 0;
      iVar3 = CONCAT22(*(undefined2 *)(iVar4 + 0x14),*(undefined2 *)(iVar4 + 0x1a));
      *(undefined1 *)(iVar4 + 0x1a) = 0;
      *(undefined1 *)(iVar4 + 0x1b) = 0;
      *(undefined1 *)(iVar4 + 0x14) = 0;
      *(undefined1 *)(iVar4 + 0x15) = 0;
      *(undefined1 *)(local_48[0] + 4) = 1;
      if (iVar3 != 0) {
        uVar2 = *(undefined4 *)(local_48[0] + 0x2c);
        cVar1 = remove_chain(local_48[0],iVar3);
        if (cVar1 == '\0') {
          *(int *)(local_48[0] + 0xc) = iVar3 + -1;
          cVar1 = move_window(local_48[0],uVar2);
        }
      }
    }
  }
  bVar5 = (byte)uVar6;
  if (cVar1 == '\0') {
    if ((int)(uVar6 << 0x1c) < 0) {
      bVar5 = bVar5 | 0x20;
    }
    param_1[7] = *(int *)(local_48[0] + 0x2c);
    param_1[8] = local_34;
  }
  if (cVar1 == '\0') {
    *(byte *)((int)param_1 + 6) = bVar5;
    param_1[4] = CONCAT22(*(undefined2 *)(local_34 + 0x14),*(undefined2 *)(local_34 + 0x1a));
    param_1[3] = (uint)*(byte *)(local_34 + 0x1e) << 0x10 | (uint)*(byte *)(local_34 + 0x1f) << 0x18
                 | (uint)*(byte *)(local_34 + 0x1d) << 8 | (uint)*(byte *)(local_34 + 0x1c);
    param_1[2] = 0;
    param_1[6] = 0;
    *param_1 = local_48[0];
    *(undefined2 *)(param_1 + 1) = *(undefined2 *)(local_48[0] + 6);
  }
  return cVar1;
}


// ==== f_read @ 08021d72

/* library (FatFs): validate, FA__ERROR (0x80) check, cluster walk with get_fat, multi-sector direct
   disk_read, dirty-buffer flush, partial sectors through fp->buf. high */

undefined8 f_read(int *param_1,int param_2,uint param_3,int *param_4)

{
  byte bVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  undefined4 local_30;
  
  *param_4 = 0;
  bVar1 = validate(*param_1,(short)param_1[1]);
  local_30._0_2_ = CONCAT11(bVar1,(undefined1)local_30);
  if (bVar1 == 0) {
    if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x18) < 0) {
      uVar2 = 2;
    }
    else if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x1f) < 0) {
      if ((uint)(param_1[3] - param_1[2]) < param_3) {
        param_3 = param_1[3] - param_1[2];
      }
      for (; param_3 != 0; param_3 = param_3 - uVar2) {
        if ((param_1[2] & 0x1ffU) == 0) {
          bVar1 = (byte)((uint)param_1[2] >> 9) & *(char *)(*param_1 + 2) - 1U;
          local_30 = CONCAT31(local_30._1_3_,bVar1);
          if (bVar1 == 0) {
            if (param_1[2] == 0) {
              uVar2 = param_1[4];
            }
            else {
              uVar2 = get_fat(*param_1,param_1[5]);
            }
            if (uVar2 < 2) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              uVar2 = 2;
              goto LAB_08021f5c;
            }
            if (uVar2 == 0xffffffff) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              uVar2 = 1;
              goto LAB_08021f5c;
            }
            param_1[5] = uVar2;
          }
          iVar3 = clust2sect(*param_1,param_1[5]);
          if (iVar3 == 0) {
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 2;
            goto LAB_08021f5c;
          }
          iVar3 = (uint)bVar1 + iVar3;
          uVar2 = param_3 >> 9;
          if (uVar2 == 0) {
            if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x19) < 0) {
              iVar4 = disk_write(*(undefined1 *)(*param_1 + 1),param_1 + 9,param_1[6],1);
              if (iVar4 != 0) {
                *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
                uVar2 = 1;
                goto LAB_08021f5c;
              }
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xbf;
            }
            if ((param_1[6] == iVar3) ||
               (iVar4 = disk_read(*(undefined1 *)(*param_1 + 1),param_1 + 9,iVar3,1), iVar4 == 0)) {
              param_1[6] = iVar3;
              goto LAB_08021f22;
            }
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 1;
            goto LAB_08021f5c;
          }
          if ((uint)*(byte *)(*param_1 + 2) < uVar2 + bVar1) {
            uVar2 = (uint)*(byte *)(*param_1 + 2) - (uint)bVar1;
          }
          iVar4 = disk_read(*(undefined1 *)(*param_1 + 1),param_2,iVar3,uVar2 & 0xff);
          if (iVar4 != 0) {
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 1;
            goto LAB_08021f5c;
          }
          if (((int)((uint)*(byte *)((int)param_1 + 6) << 0x19) < 0) &&
             ((uint)(param_1[6] - iVar3) < uVar2)) {
            mem_cpy((param_1[6] - iVar3) * 0x200 + param_2,param_1 + 9,0x200);
          }
          uVar2 = uVar2 * 0x200;
        }
        else {
LAB_08021f22:
          uVar2 = 0x200 - (param_1[2] & 0x1ffU);
          if (param_3 < uVar2) {
            uVar2 = param_3;
          }
          mem_cpy(param_2,(int)param_1 + (param_1[2] & 0x1ffU) + 0x24,uVar2);
        }
        param_2 = param_2 + uVar2;
        param_1[2] = uVar2 + param_1[2];
        *param_4 = uVar2 + *param_4;
      }
      uVar2 = 0;
    }
    else {
      uVar2 = 7;
    }
  }
  else {
    uVar2 = (uint)bVar1;
  }
LAB_08021f5c:
  return CONCAT44(local_30,uVar2);
}


// ==== f_write @ 08021f60

/* library (FatFs): validate, FA__ERROR/FA_WRITE checks, create_chain on cluster boundary (stops on
   disk full), multi-sector direct disk_write, sets FA__WRITTEN (0x20) / FA__DIRTY (0x40), grows
   fsize. high (emulated) */

undefined8 f_write(int *param_1,int param_2,uint param_3,int *param_4)

{
  byte bVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  undefined4 local_30;
  
  *param_4 = 0;
  bVar1 = validate(*param_1,(short)param_1[1]);
  local_30._0_2_ = CONCAT11(bVar1,(undefined1)local_30);
  if (bVar1 == 0) {
    if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x18) < 0) {
      uVar2 = 2;
    }
    else if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x1e) < 0) {
      if (param_3 + param_1[3] < (uint)param_1[3]) {
        param_3 = 0;
      }
      for (; param_3 != 0; param_3 = param_3 - uVar2) {
        if ((param_1[2] & 0x1ffU) == 0) {
          bVar1 = (byte)((uint)param_1[2] >> 9) & *(char *)(*param_1 + 2) - 1U;
          local_30 = CONCAT31(local_30._1_3_,bVar1);
          if (bVar1 == 0) {
            if (param_1[2] == 0) {
              iVar3 = param_1[4];
              if (iVar3 == 0) {
                iVar3 = create_chain(*param_1,0);
                param_1[4] = iVar3;
              }
            }
            else {
              iVar3 = create_chain(*param_1,param_1[5]);
            }
            if (iVar3 == 0) break;
            if (iVar3 == 1) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              uVar2 = 2;
              goto LAB_08022184;
            }
            if (iVar3 == -1) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              uVar2 = 1;
              goto LAB_08022184;
            }
            param_1[5] = iVar3;
          }
          if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x19) < 0) {
            iVar3 = disk_write(*(undefined1 *)(*param_1 + 1),param_1 + 9,param_1[6],1);
            if (iVar3 != 0) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              uVar2 = 1;
              goto LAB_08022184;
            }
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xbf;
          }
          iVar3 = clust2sect(*param_1,param_1[5]);
          if (iVar3 == 0) {
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 2;
            goto LAB_08022184;
          }
          iVar3 = (uint)bVar1 + iVar3;
          uVar2 = param_3 >> 9;
          if (uVar2 == 0) {
            if (((param_1[6] == iVar3) || ((uint)param_1[3] <= (uint)param_1[2])) ||
               (iVar4 = disk_read(*(undefined1 *)(*param_1 + 1),param_1 + 9,iVar3,1), iVar4 == 0)) {
              param_1[6] = iVar3;
              goto LAB_0802212a;
            }
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 1;
            goto LAB_08022184;
          }
          if ((uint)*(byte *)(*param_1 + 2) < uVar2 + bVar1) {
            uVar2 = (uint)*(byte *)(*param_1 + 2) - (uint)bVar1;
          }
          iVar4 = disk_write(*(undefined1 *)(*param_1 + 1),param_2,iVar3,uVar2 & 0xff);
          if (iVar4 != 0) {
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            uVar2 = 1;
            goto LAB_08022184;
          }
          if ((uint)(param_1[6] - iVar3) < uVar2) {
            mem_cpy(param_1 + 9,param_2 + (param_1[6] - iVar3) * 0x200,0x200);
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xbf;
          }
          uVar2 = uVar2 * 0x200;
        }
        else {
LAB_0802212a:
          uVar2 = 0x200 - (param_1[2] & 0x1ffU);
          if (param_3 < uVar2) {
            uVar2 = param_3;
          }
          mem_cpy((int)param_1 + (param_1[2] & 0x1ffU) + 0x24,param_2,uVar2);
          *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x40;
        }
        param_2 = param_2 + uVar2;
        param_1[2] = uVar2 + param_1[2];
        *param_4 = uVar2 + *param_4;
      }
      if ((uint)param_1[3] < (uint)param_1[2]) {
        param_1[3] = param_1[2];
      }
      *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x20;
      uVar2 = 0;
    }
    else {
      uVar2 = 7;
    }
  }
  else {
    uVar2 = (uint)bVar1;
  }
LAB_08022184:
  return CONCAT44(local_30,uVar2);
}


// ==== f_sync @ 08022188

/* library (FatFs): validate, if FA__WRITTEN: flush FA__DIRTY buffer, update entry (AM_ARC, fsize,
   sclust lo/hi, write time = get_fattime constant 2006-02-09), clear flag, sync. Called by f_close
   (0802224c). high */

undefined8 f_sync(int *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  int iVar2;
  uint uVar3;
  undefined4 uVar4;
  
  bVar1 = validate(*param_1,(short)param_1[1]);
  if ((bVar1 == 0) && ((int)((uint)*(byte *)((int)param_1 + 6) << 0x1a) < 0)) {
    if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x19) < 0) {
      iVar2 = disk_write(*(undefined1 *)(*param_1 + 1),param_1 + 9,param_1[6],1);
      if (iVar2 != 0) {
        uVar3 = 1;
        goto LAB_0802224a;
      }
      *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xbf;
    }
    bVar1 = move_window(*param_1,param_1[7]);
    if (bVar1 == 0) {
      iVar2 = param_1[8];
      *(byte *)(iVar2 + 0xb) = *(byte *)(iVar2 + 0xb) | 0x20;
      *(char *)(iVar2 + 0x1c) = (char)param_1[3];
      *(char *)(iVar2 + 0x1d) = (char)((uint)param_1[3] >> 8);
      *(char *)(iVar2 + 0x1e) = (char)((uint)param_1[3] >> 0x10);
      *(char *)(iVar2 + 0x1f) = (char)((uint)param_1[3] >> 0x18);
      *(char *)(iVar2 + 0x1a) = (char)param_1[4];
      *(char *)(iVar2 + 0x1b) = (char)((uint)param_1[4] >> 8);
      *(char *)(iVar2 + 0x14) = (char)((uint)param_1[4] >> 0x10);
      *(char *)(iVar2 + 0x15) = (char)((uint)param_1[4] >> 0x18);
      uVar4 = get_fattime();
      *(char *)(iVar2 + 0x16) = (char)uVar4;
      *(char *)(iVar2 + 0x17) = (char)((uint)uVar4 >> 8);
      *(char *)(iVar2 + 0x18) = (char)((uint)uVar4 >> 0x10);
      *(char *)(iVar2 + 0x19) = (char)((uint)uVar4 >> 0x18);
      *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xdf;
      *(undefined1 *)(*param_1 + 4) = 1;
      bVar1 = sync(*param_1);
    }
  }
  uVar3 = (uint)bVar1;
LAB_0802224a:
  return CONCAT44(param_4,uVar3);
}


// ==== f_close @ 0802224c

/* library (FatFs): f_sync, then fp->fs = 0 on FR_OK. high */

ulonglong f_close(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  uVar1 = f_sync(param_1);
  if ((uVar1 & 0xff) == 0) {
    *param_1 = 0;
  }
  return CONCAT44(param_4,uVar1) & 0xffffffff000000ff;
}


// ==== f_lseek @ 08022268

/* library (FatFs): no fast-seek; clips to fsize unless FA_WRITE, follows or extends (create_chain)
   the chain, reloads fp->buf sector. Log_VerifyWrite seeks back with it. high (emulated) */

char f_lseek(int *param_1,uint param_2)

{
  byte bVar1;
  char cVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  
  cVar2 = validate(*param_1,(short)param_1[1]);
  if (cVar2 == '\0') {
    if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x18) < 0) {
      cVar2 = '\x02';
    }
    else {
      if (((uint)param_1[3] < param_2) && (-1 < (int)((uint)*(byte *)((int)param_1 + 6) << 0x1e))) {
        param_2 = param_1[3];
      }
      iVar3 = param_1[2];
      iVar6 = 0;
      param_1[2] = 0;
      if (param_2 != 0) {
        bVar1 = *(byte *)(*param_1 + 2);
        uVar4 = (uint)bVar1 * 0x200;
        if ((iVar3 == 0) || ((param_2 - 1) / uVar4 < (iVar3 - 1U) / uVar4)) {
          uVar5 = param_1[4];
          if (uVar5 == 0) {
            uVar5 = create_chain(*param_1,0);
            if (uVar5 == 1) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              return '\x02';
            }
            if (uVar5 == 0xffffffff) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              return '\x01';
            }
            param_1[4] = uVar5;
          }
          param_1[5] = uVar5;
        }
        else {
          param_1[2] = iVar3 - 1U & ~(uVar4 - 1);
          param_2 = param_2 - param_1[2];
          uVar5 = param_1[5];
        }
        if (uVar5 != 0) {
          for (; uVar4 < param_2; param_2 = param_2 + (uint)bVar1 * -0x200) {
            if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x1e) < 0) {
              uVar5 = create_chain(*param_1,uVar5);
              if (uVar5 == 0) {
                param_2 = uVar4;
                break;
              }
            }
            else {
              uVar5 = get_fat(*param_1,uVar5);
            }
            if (uVar5 == 0xffffffff) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              return '\x01';
            }
            if ((uVar5 < 2) || (*(uint *)(*param_1 + 0x18) <= uVar5)) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              return '\x02';
            }
            param_1[5] = uVar5;
            param_1[2] = uVar4 + param_1[2];
          }
          param_1[2] = param_2 + param_1[2];
          if ((param_2 & 0x1ff) != 0) {
            iVar6 = clust2sect(*param_1,uVar5);
            if (iVar6 == 0) {
              *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
              return '\x02';
            }
            iVar6 = iVar6 + (param_2 >> 9);
          }
        }
      }
      if (((param_1[2] & 0x1ffU) != 0) && (iVar6 != param_1[6])) {
        if ((int)((uint)*(byte *)((int)param_1 + 6) << 0x19) < 0) {
          iVar3 = disk_write(*(undefined1 *)(*param_1 + 1),param_1 + 9,param_1[6],1);
          if (iVar3 != 0) {
            *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
            return '\x01';
          }
          *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) & 0xbf;
        }
        iVar3 = disk_read(*(undefined1 *)(*param_1 + 1),param_1 + 9,iVar6,1);
        if (iVar3 != 0) {
          *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x80;
          return '\x01';
        }
        param_1[6] = iVar6;
      }
      if ((uint)param_1[3] < (uint)param_1[2]) {
        param_1[3] = param_1[2];
        *(byte *)((int)param_1 + 6) = *(byte *)((int)param_1 + 6) | 0x20;
      }
    }
  }
  return cVar2;
}


// ==== f_opendir @ 0802242c

/* library (FatFs): chk_mounted, follow_path, must be a directory (else FR_NO_PATH 5), id from fs,
   dir_sdi(0); Log_FindNextIndex opens "0:". high */

char f_opendir(int *param_1,undefined4 param_2)

{
  char cVar1;
  undefined1 auStack_20 [16];
  undefined4 local_10;
  
  local_10 = param_2;
  cVar1 = chk_mounted(&local_10,param_1,0);
  if (cVar1 == '\0') {
    param_1[6] = (int)auStack_20;
    cVar1 = follow_path(param_1,local_10);
    if (cVar1 == '\0') {
      if (param_1[5] != 0) {
        if ((int)((uint)*(byte *)(param_1[5] + 0xb) << 0x1b) < 0) {
          param_1[2] = CONCAT22(CONCAT11(*(undefined1 *)(param_1[5] + 0x15),
                                         *(undefined1 *)(param_1[5] + 0x14)),
                                CONCAT11(*(undefined1 *)(param_1[5] + 0x1b),
                                         *(undefined1 *)(param_1[5] + 0x1a)));
        }
        else {
          cVar1 = '\x05';
        }
      }
      if (cVar1 == '\0') {
        *(undefined2 *)(param_1 + 1) = *(undefined2 *)(*param_1 + 6);
        cVar1 = dir_sdi(param_1,0);
      }
    }
    if (cVar1 == '\x04') {
      cVar1 = '\x05';
    }
  }
  return cVar1;
}


// ==== f_readdir @ 080224b6

/* library (FatFs): validate; NULL fno rewinds, else dir_read + get_fileinfo + dir_next; end of dir
   gives an empty name with FR_OK. high */

char f_readdir(undefined4 *param_1,int param_2)

{
  char cVar1;
  undefined1 auStack_20 [16];
  
  cVar1 = validate(*param_1,*(undefined2 *)(param_1 + 1));
  if (cVar1 == '\0') {
    if (param_2 == 0) {
      cVar1 = dir_sdi(param_1,0);
    }
    else {
      param_1[6] = auStack_20;
      cVar1 = dir_read(param_1);
      if (cVar1 == '\x04') {
        param_1[4] = 0;
        cVar1 = '\0';
      }
      if (cVar1 == '\0') {
        get_fileinfo(param_1,param_2);
        cVar1 = dir_next(param_1,0);
        if (cVar1 == '\x04') {
          param_1[4] = 0;
          cVar1 = '\0';
        }
      }
    }
  }
  return cVar1;
}


// ==== f_stat @ 08022526

/* library (FatFs): chk_mounted, follow_path, get_fileinfo; root gives FR_INVALID_NAME 6;
   File_SetTimestamp uses it before f_utime. high */

char f_stat(undefined4 param_1,undefined4 param_2)

{
  char cVar1;
  undefined1 auStack_38 [12];
  undefined1 auStack_2c [20];
  int local_18;
  undefined1 *local_14;
  undefined4 local_10;
  
  local_10 = param_1;
  cVar1 = chk_mounted(&local_10,auStack_2c,0);
  if (cVar1 == '\0') {
    local_14 = auStack_38;
    cVar1 = follow_path(auStack_2c,local_10);
    if (cVar1 == '\0') {
      if (local_18 == 0) {
        cVar1 = '\x06';
      }
      else {
        get_fileinfo(auStack_2c,param_2);
      }
    }
  }
  return cVar1;
}


// ==== f_utime @ 0802256e

/* library (FatFs, _USE_CHMOD): writes fno->ftime/fdate into DIR_WrtTime (0x16) / DIR_WrtDate
   (0x18), wflag, sync; called by File_SetTimestamp. high */

char f_utime(undefined4 param_1,int param_2)

{
  char cVar1;
  int local_40 [5];
  int local_2c;
  undefined1 *local_28;
  undefined1 auStack_24 [16];
  undefined4 local_14;
  
  local_14 = param_1;
  cVar1 = chk_mounted(&local_14,local_40,1);
  if (cVar1 == '\0') {
    local_28 = auStack_24;
    cVar1 = follow_path(local_40,local_14);
    if (cVar1 == '\0') {
      if (local_2c == 0) {
        cVar1 = '\x06';
      }
      else {
        *(char *)(local_2c + 0x16) = (char)*(undefined2 *)(param_2 + 6);
        *(char *)(local_2c + 0x17) = (char)((ushort)*(undefined2 *)(param_2 + 6) >> 8);
        *(char *)(local_2c + 0x18) = (char)*(undefined2 *)(param_2 + 4);
        *(char *)(local_2c + 0x19) = (char)((ushort)*(undefined2 *)(param_2 + 4) >> 8);
        *(undefined1 *)(local_40[0] + 4) = 1;
        cVar1 = sync(local_40[0]);
      }
    }
  }
  return cVar1;
}


// ==== SD_Init @ 080225d8

/* ST sdio_sd.c: SD_LowLevel_Init, SDIO_DeInit (08024074), SD_PowerON, SD_InitializeCards, SDIO_Init
   ClockDiv 0 (bypass off), SD_GetCardInfo, SD_SelectDeselect (08022f96, RCA << 16),
   SD_EnableWideBusOperation(1-bit). Called by disk_initialize (08023f5c) and the SD re-init path.
   high */

undefined8 SD_Init(void)

{
  undefined3 uVar1;
  byte bVar2;
  uint uVar3;
  undefined4 unaff_r7;
  undefined4 uVar4;
  
  uVar1 = (undefined3)((uint)unaff_r7 >> 8);
  SD_LowLevel_Init();
  SDIO_DeInit();
  bVar2 = SD_PowerON();
  uVar4 = CONCAT31(uVar1,bVar2);
  if (bVar2 == 0) {
    bVar2 = SD_InitializeCards();
    uVar4 = CONCAT31(uVar1,bVar2);
    if (bVar2 == 0) {
      DAT_20007b64 = 1;
      DAT_20007b50 = 0;
      DAT_20007b54 = 0;
      DAT_20007b58 = 0;
      DAT_20007b5c = 0;
      DAT_20007b60 = 0;
      SDIO_Init(&DAT_20007b50);
      bVar2 = SD_GetCardInfo(&DAT_20007698);
      if (bVar2 == 0) {
        bVar2 = SD_SelectDeselect((uint)DAT_200076ec << 0x10);
      }
      if (bVar2 == 0) {
        bVar2 = SD_EnableWideBusOperation(0);
      }
      uVar4 = CONCAT31(uVar1,bVar2);
      uVar3 = (uint)bVar2;
    }
    else {
      uVar3 = (uint)bVar2;
    }
  }
  else {
    uVar3 = (uint)bVar2;
  }
  return CONCAT44(uVar4,uVar3);
}


// ==== SD_GetStatus @ 0802268a

/* ST sdio_sd.c: SD_GetState (named SD_DiskStatus 080226ac) == 4 TRANSFER -> 0 OK, 0xFF ERROR -> 2,
   else 1 BUSY; disk_read/disk_write poll it. high */

undefined4 SD_GetStatus(void)

{
  char cVar1;
  undefined4 uVar2;
  
  cVar1 = SD_GetState();
  if (cVar1 == '\x04') {
    uVar2 = 0;
  }
  else if (cVar1 == -1) {
    uVar2 = 2;
  }
  else {
    uVar2 = 1;
  }
  return uVar2;
}


// ==== SD_GetState @ 080226ac

/* ST SD_GetState: SD_Detect (always present), SD_SendStatus (CMD13), returns card state (status >>
   9) & 0xF (4 = TRAN), 0xFF on error (shift checked in the disassembly; was SD_DiskStatus). high */

undefined4 SD_GetState(void)

{
  int iVar1;
  undefined4 uVar2;
  undefined1 auStack_88 [128];
  
  iVar1 = SD_Detect();
  if (iVar1 == 1) {
    iVar1 = SD_SendStatus(auStack_88 + 0x80);
    if (iVar1 == 0) {
      uVar2 = 0;
    }
    else {
      uVar2 = 0xff;
    }
  }
  else {
    uVar2 = 0xff;
  }
  return uVar2;
}


// ==== SD_Detect @ 080226d6

/* returns 1 = present: the board has no card-detect switch (no net in the schematic). high */

undefined4 SD_Detect(void)

{
  return 1;
}


// ==== SD_PowerON @ 080226e6

/* ST sdio_sd.c: SDIO init 400 kHz, power on, CLKEN, CMD0, CMD8 (0x1AA; answer = SD 2.0, HCS), CMD55
   + ACMD41 loop (0xFFFF tries). high (emulated) */

undefined8 SD_PowerON(void)

{
  undefined3 uVar1;
  byte bVar2;
  char cVar3;
  undefined4 in_r3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint local_18;
  
  uVar1 = (undefined3)((uint)in_r3 >> 8);
  uVar4 = 0;
  uVar5 = 0;
  uVar6 = 0;
  uVar7 = 0;
  DAT_20007b64 = 0x76;
  DAT_20007b50 = 0;
  DAT_20007b54 = 0;
  DAT_20007b58 = 0;
  DAT_20007b5c = 0;
  DAT_20007b60 = 0;
  SDIO_Init(&DAT_20007b50);
  SDIO_SetPowerState(3);
  SDIO_ClockCmd(1);
  DAT_20007bbc = 0;
  DAT_20007bc0 = 0;
  DAT_20007bc4 = 0;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  SDIO_SendCommand(&DAT_20007bbc);
  bVar2 = SD_CmdError();
  local_18 = CONCAT31(uVar1,bVar2);
  if (bVar2 == 0) {
    DAT_20007bbc = 0x1aa;
    DAT_20007bc0 = 8;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    SDIO_SendCommand(&DAT_20007bbc);
    cVar3 = SD_CmdResp7Error();
    if (cVar3 == '\0') {
      DAT_20007ee8 = 1;
      uVar7 = 0x40000000;
    }
    else {
      DAT_20007bbc = 0;
      DAT_20007bc0 = 0x37;
      DAT_20007bc4 = 0x40;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      SD_CmdResp1Error(0x37);
    }
    DAT_20007bbc = 0;
    DAT_20007bc0 = 0x37;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    SDIO_SendCommand(&DAT_20007bbc);
    cVar3 = SD_CmdResp1Error(0x37);
    local_18 = CONCAT31(uVar1,cVar3);
    if (cVar3 == '\0') {
      for (; (uVar6 == 0 && (uVar5 < 0xffff)); uVar5 = uVar5 + 1) {
        DAT_20007bbc = 0;
        DAT_20007bc0 = 0x37;
        DAT_20007bc4 = 0x40;
        DAT_20007bc8 = 0;
        DAT_20007bcc = 0x400;
        SDIO_SendCommand(&DAT_20007bbc);
        bVar2 = SD_CmdResp1Error(0x37);
        local_18 = CONCAT31(local_18._1_3_,bVar2);
        if (bVar2 != 0) {
          uVar4 = (uint)bVar2;
          goto LAB_08022930;
        }
        DAT_20007bbc = uVar7 | 0x80100000;
        DAT_20007bc0 = 0x29;
        DAT_20007bc4 = 0x40;
        DAT_20007bc8 = 0;
        DAT_20007bcc = 0x400;
        SDIO_SendCommand(&DAT_20007bbc);
        bVar2 = SD_CmdResp3Error();
        local_18 = CONCAT31(local_18._1_3_,bVar2);
        if (bVar2 != 0) {
          uVar4 = (uint)bVar2;
          goto LAB_08022930;
        }
        uVar4 = SDIO_GetResponse(0);
        uVar6 = uVar4 >> 0x1f;
      }
      if (0xfffe < uVar5) {
        local_18 = CONCAT31(local_18._1_3_,0x1b);
        uVar4 = 0x1b;
        goto LAB_08022930;
      }
      if ((uVar4 & 0x40000000) != 0) {
        DAT_20007ee8 = 2;
      }
    }
    uVar4 = local_18 & 0xff;
  }
  else {
    uVar4 = (uint)bVar2;
  }
LAB_08022930:
  return CONCAT44(local_18,uVar4);
}


// ==== SD_PowerOFF @ 08022932

/* ST sdio_sd.c: SDIO_SetPowerState(OFF), returns SD_OK; after CalFile_Save/Load, Log_Write and in
   main. high */

undefined4 SD_PowerOFF(void)

{
  SDIO_SetPowerState(0);
  return 0;
}


// ==== SD_InitializeCards @ 08022942

/* ST sdio_sd.c: needs SDIO powered (else SD_REQUEST_NOT_APPLICABLE 0x25), CMD2 (CID to
   0x20007c00..), CMD3 (RCA via SD_CmdResp6Error, stored 0x20007eec), CMD9 (CSD to 0x20007bf0..).
   high */

undefined8 SD_InitializeCards(void)

{
  byte bVar1;
  int iVar2;
  uint uVar3;
  uint local_10 [2];
  
  local_10[0] = CONCAT22(local_10[0]._2_2_,1);
  iVar2 = SDIO_GetPowerState();
  if (iVar2 == 0) {
    uVar3 = 0x25;
  }
  else {
    if (DAT_20007ee8 != 4) {
      DAT_20007bbc = 0;
      DAT_20007bc0 = 2;
      DAT_20007bc4 = 0xc0;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      bVar1 = SD_CmdResp2Error();
      if (bVar1 != 0) {
        uVar3 = (uint)bVar1;
        goto LAB_08022ae2;
      }
      DAT_20007c00 = SDIO_GetResponse(0);
      DAT_20007c04 = SDIO_GetResponse(4);
      DAT_20007c08 = SDIO_GetResponse(8);
      DAT_20007c0c = SDIO_GetResponse(0xc);
    }
    if ((((DAT_20007ee8 == 0) || (DAT_20007ee8 == 1)) || (DAT_20007ee8 == 6)) || (DAT_20007ee8 == 2)
       ) {
      DAT_20007bbc = 0;
      DAT_20007bc0 = 3;
      DAT_20007bc4 = 0x40;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      bVar1 = SD_CmdResp6Error(3,local_10);
      if (bVar1 != 0) {
        uVar3 = (uint)bVar1;
        goto LAB_08022ae2;
      }
    }
    if (DAT_20007ee8 != 4) {
      DAT_20007eec = local_10[0] & 0xffff;
      DAT_20007bbc = local_10[0] << 0x10;
      DAT_20007bc0 = 9;
      DAT_20007bc4 = 0xc0;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      bVar1 = SD_CmdResp2Error();
      if (bVar1 != 0) {
        uVar3 = (uint)bVar1;
        goto LAB_08022ae2;
      }
      DAT_20007bf0 = SDIO_GetResponse(0);
      DAT_20007bf4 = SDIO_GetResponse(4);
      DAT_20007bf8 = SDIO_GetResponse(8);
      DAT_20007bfc = SDIO_GetResponse(0xc);
    }
    uVar3 = 0;
  }
LAB_08022ae2:
  return CONCAT44(local_10[0],uVar3);
}


// ==== SD_GetCardInfo @ 08022ae4

/* ST sdio_sd.c: unpacks CSD/CID into SD_CardInfo at 0x20007698 (CardCapacity uint64 at +0x48,
   CardBlockSize +0x50, RCA +0x54, CardType +0x56); SDHC capacity (C_SIZE+1)*512 KiB. high */

undefined4 SD_GetCardInfo(byte *param_1)

{
  longlong lVar1;
  byte bVar2;
  uint uVar3;
  uint uVar4;
  uint uVar5;
  
  param_1[0x56] = (byte)DAT_20007ee8;
  *(short *)(param_1 + 0x54) = (short)DAT_20007eec;
  uVar3 = DAT_20007bf0 >> 0x18;
  bVar2 = (byte)(DAT_20007bf0 >> 0x18);
  *param_1 = bVar2 >> 6;
  param_1[1] = (byte)((uVar3 & 0x3f) >> 2);
  param_1[2] = bVar2 & 3;
  param_1[3] = (byte)(DAT_20007bf0 >> 0x10);
  param_1[4] = (byte)(DAT_20007bf0 >> 8);
  param_1[5] = (byte)DAT_20007bf0;
  *(short *)(param_1 + 6) = (short)((DAT_20007bf4 >> 0x18) << 4);
  uVar3 = DAT_20007bf4;
  *(ushort *)(param_1 + 6) =
       (ushort)(byte)((DAT_20007bf4 >> 0x10 & 0xff) >> 4) | *(ushort *)(param_1 + 6);
  param_1[8] = (byte)(uVar3 >> 0x10) & 0xf;
  uVar3 = DAT_20007bf4 >> 8;
  bVar2 = (byte)(DAT_20007bf4 >> 8);
  param_1[9] = bVar2 >> 7;
  param_1[10] = (byte)((uVar3 & 0x7f) >> 6);
  param_1[0xb] = (byte)((uVar3 & 0x3f) >> 5);
  param_1[0xc] = (byte)((uVar3 & 0x1f) >> 4);
  param_1[0xd] = 0;
  if ((DAT_20007ee8 == 0) || (DAT_20007ee8 == 1)) {
    *(uint *)(param_1 + 0x10) = (bVar2 & 3) << 10;
    *(uint *)(param_1 + 0x10) = *(uint *)(param_1 + 0x10) | (DAT_20007bf4 & 0xff) << 2;
    uVar3 = DAT_20007bf8;
    uVar4 = DAT_20007bf8 >> 0x18;
    *(uint *)(param_1 + 0x10) = DAT_20007bf8 >> 0x1e | *(uint *)(param_1 + 0x10);
    param_1[0x14] = (byte)((uVar4 & 0x3f) >> 3);
    param_1[0x15] = (byte)(uVar3 >> 0x18) & 7;
    uVar3 = DAT_20007bf8 >> 0x10;
    param_1[0x16] = (byte)((DAT_20007bf8 >> 0x10 & 0xff) >> 5);
    param_1[0x17] = (byte)((uVar3 & 0x1f) >> 2);
    param_1[0x18] = (byte)((uVar3 & 3) << 1);
    uVar3 = DAT_20007bf8 >> 8;
    param_1[0x18] = param_1[0x18] | (byte)((DAT_20007bf8 >> 8 & 0xff) >> 7);
    *(int *)(param_1 + 0x48) = *(int *)(param_1 + 0x10) + 1;
    param_1[0x4c] = 0;
    param_1[0x4d] = 0;
    param_1[0x4e] = 0;
    param_1[0x4f] = 0;
    uVar5 = *(uint *)(param_1 + 0x48);
    uVar4 = 1 << (param_1[0x18] + 2 & 0xff);
    lVar1 = (ulonglong)uVar4 * (ulonglong)uVar5;
    *(int *)(param_1 + 0x48) = (int)lVar1;
    *(uint *)(param_1 + 0x4c) =
         ((int)uVar4 >> 0x1f) * uVar5 +
         uVar4 * *(int *)(param_1 + 0x4c) + (int)((ulonglong)lVar1 >> 0x20);
    *(int *)(param_1 + 0x50) = 1 << (uint)param_1[8];
    lVar1 = (ulonglong)*(uint *)(param_1 + 0x50) * (ulonglong)*(uint *)(param_1 + 0x48);
    *(int *)(param_1 + 0x48) = (int)lVar1;
    *(uint *)(param_1 + 0x4c) =
         *(uint *)(param_1 + 0x50) * *(int *)(param_1 + 0x4c) + (int)((ulonglong)lVar1 >> 0x20);
  }
  else if (DAT_20007ee8 == 2) {
    *(uint *)(param_1 + 0x10) = (DAT_20007bf4 & 0x3f) << 0x10;
    *(uint *)(param_1 + 0x10) = *(uint *)(param_1 + 0x10) | (DAT_20007bf8 >> 0x18) << 8;
    *(uint *)(param_1 + 0x10) = *(uint *)(param_1 + 0x10) | DAT_20007bf8 >> 0x10 & 0xff;
    uVar3 = DAT_20007bf8 >> 8;
    *(int *)(param_1 + 0x48) = (*(int *)(param_1 + 0x10) + 1) * 0x80000;
    param_1[0x4c] = 0;
    param_1[0x4d] = 0;
    param_1[0x4e] = 0;
    param_1[0x4f] = 0;
    param_1[0x50] = 0;
    param_1[0x51] = 2;
    param_1[0x52] = 0;
    param_1[0x53] = 0;
  }
  param_1[0x19] = (byte)((uVar3 & 0x7f) >> 6);
  param_1[0x1a] = (byte)((uVar3 & 0x3f) << 1);
  uVar3 = DAT_20007bf8;
  param_1[0x1a] = param_1[0x1a] | (byte)((DAT_20007bf8 & 0xff) >> 7);
  param_1[0x1b] = (byte)uVar3 & 0x7f;
  uVar3 = DAT_20007bfc >> 0x18;
  param_1[0x1c] = (byte)(DAT_20007bfc >> 0x1f);
  param_1[0x1d] = (byte)((uVar3 & 0x7f) >> 5);
  param_1[0x1e] = (byte)((uVar3 & 0x1f) >> 2);
  param_1[0x1f] = (byte)((uVar3 & 3) << 2);
  uVar3 = DAT_20007bfc;
  uVar4 = DAT_20007bfc >> 0x10;
  param_1[0x1f] = param_1[0x1f] | (byte)((DAT_20007bfc >> 0x10 & 0xff) >> 6);
  param_1[0x20] = (byte)((uVar4 & 0x3f) >> 5);
  param_1[0x21] = 0;
  param_1[0x22] = (byte)(uVar3 >> 0x10) & 1;
  uVar3 = DAT_20007bfc;
  uVar4 = DAT_20007bfc >> 8;
  param_1[0x23] = (byte)((DAT_20007bfc >> 8 & 0xff) >> 7);
  param_1[0x24] = (byte)((uVar4 & 0x7f) >> 6);
  param_1[0x25] = (byte)((uVar4 & 0x3f) >> 5);
  param_1[0x26] = (byte)((uVar4 & 0x1f) >> 4);
  param_1[0x27] = (byte)((uVar4 & 0xf) >> 2);
  param_1[0x28] = (byte)(uVar3 >> 8) & 3;
  param_1[0x29] = (byte)((DAT_20007bfc & 0xff) >> 1);
  param_1[0x2a] = 1;
  param_1[0x2c] = (byte)(DAT_20007c00 >> 0x18);
  *(short *)(param_1 + 0x2e) = (short)((DAT_20007c00 >> 0x10 & 0xff) << 8);
  *(ushort *)(param_1 + 0x2e) = *(ushort *)(param_1 + 0x2e) | (ushort)(byte)(DAT_20007c00 >> 8);
  *(uint *)(param_1 + 0x30) = DAT_20007c00 << 0x18;
  *(uint *)(param_1 + 0x30) = *(uint *)(param_1 + 0x30) | (DAT_20007c04 >> 0x18) << 0x10;
  *(uint *)(param_1 + 0x30) = *(uint *)(param_1 + 0x30) | (DAT_20007c04 >> 0x10 & 0xff) << 8;
  *(uint *)(param_1 + 0x30) = *(uint *)(param_1 + 0x30) | DAT_20007c04 >> 8 & 0xff;
  param_1[0x34] = (byte)DAT_20007c04;
  param_1[0x35] = (byte)(DAT_20007c08 >> 0x18);
  *(uint *)(param_1 + 0x38) = (DAT_20007c08 >> 0x10) << 0x18;
  *(uint *)(param_1 + 0x38) = *(uint *)(param_1 + 0x38) | (DAT_20007c08 >> 8 & 0xff) << 0x10;
  *(uint *)(param_1 + 0x38) = *(uint *)(param_1 + 0x38) | (DAT_20007c08 & 0xff) << 8;
  *(uint *)(param_1 + 0x38) = *(uint *)(param_1 + 0x38) | DAT_20007c0c >> 0x18;
  bVar2 = (byte)(DAT_20007c0c >> 0x10);
  param_1[0x3c] = param_1[0x3c] | bVar2 >> 4;
  *(ushort *)(param_1 + 0x3e) = (bVar2 & 0xf) << 8;
  *(ushort *)(param_1 + 0x3e) = *(ushort *)(param_1 + 0x3e) | (ushort)(byte)(DAT_20007c0c >> 8);
  param_1[0x40] = (byte)((DAT_20007c0c & 0xff) >> 1);
  param_1[0x41] = 1;
  return 0;
}


// ==== SD_EnableWideBusOperation @ 08022ece

/* ST sdio_sd.c: MMC (CardType 3) and 8-bit give 0x27 SD_UNSUPPORTED_FEATURE; 0x800 = 4-bit via
   SDEnWideBus(1); else 1-bit; re-runs SDIO_Init with the bus width. Called with 0 (1-bit board).
   high */

undefined8
SD_EnableWideBusOperation(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  uint uVar1;
  
  uVar1 = 0;
  if (DAT_20007ee8 == 3) {
    uVar1 = 0x27;
  }
  else if (((DAT_20007ee8 == 0) || (DAT_20007ee8 == 1)) || (DAT_20007ee8 == 2)) {
    if (param_1 == 0x1000) {
      uVar1 = 0x27;
    }
    else if (param_1 == 0x800) {
      uVar1 = SDEnWideBus(1);
      uVar1 = uVar1 & 0xff;
      if (uVar1 == 0) {
        DAT_20007b64 = 1;
        DAT_20007b50 = 0;
        DAT_20007b54 = 0;
        DAT_20007b58 = 0;
        DAT_20007b5c = 0x800;
        DAT_20007b60 = 0;
        SDIO_Init(&DAT_20007b50);
      }
    }
    else {
      uVar1 = SDEnWideBus(0);
      uVar1 = uVar1 & 0xff;
      if (uVar1 == 0) {
        DAT_20007b64 = 1;
        DAT_20007b50 = 0;
        DAT_20007b54 = 0;
        DAT_20007b58 = 0;
        DAT_20007b5c = 0;
        DAT_20007b60 = 0;
        SDIO_Init(&DAT_20007b50);
      }
    }
  }
  return CONCAT44(param_4,uVar1);
}


// ==== SD_SelectDeselect @ 08022f96

/* ST sdio_sd.c: CMD7 (arg = RCA << 16), short response, CPSM on, SD_CmdResp1Error(7); from SD_Init.
   high */

ulonglong SD_SelectDeselect(undefined4 param_1,undefined4 param_2,undefined4 param_3,
                           undefined4 param_4)

{
  undefined4 uVar1;
  
  DAT_20007bc0 = 7;
  DAT_20007bc4 = 0x40;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  DAT_20007bbc = param_1;
  SDIO_SendCommand(&DAT_20007bbc);
  uVar1 = SD_CmdResp1Error(7);
  return CONCAT44(param_4,uVar1) & 0xffffffff000000ff;
}


// ==== SD_ReadBlock @ 08022fce  [SDIO]

/* CMD16 (512), data config, CMD17, SDIO IT mask 0x32A, DMA on, DMA2 ch4 RX; SDHC block address =
   byte address >> 9. high (emulated) */

char SD_ReadBlock(undefined4 param_1,undefined4 param_2,uint param_3,int param_4,ushort param_5)

{
  char cVar1;
  
  DAT_200080f7 = 0;
  DAT_20007ef4 = 0;
  DAT_20007ef0 = 0;
  SDIO.DCTRL = 0;
  if (DAT_20007ee8 == 2) {
    param_5 = 0x200;
    param_3 = param_3 >> 9 | param_4 << 0x17;
  }
  DAT_20007bbc = (uint)param_5;
  DAT_20007bc0 = 0x10;
  DAT_20007bc4 = 0x40;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  SDIO_SendCommand(&DAT_20007bbc);
  cVar1 = SD_CmdResp1Error(0x10);
  if (cVar1 == '\0') {
    DAT_20007b68 = 0xffffffff;
    DAT_20007b6c = (uint)param_5;
    DAT_20007b70 = 0x90;
    DAT_20007b74 = 2;
    DAT_20007b78 = 0;
    DAT_20007b7c = 1;
    SDIO_DataConfig(&DAT_20007b68);
    DAT_20007bc0 = 0x11;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    DAT_20007bbc = param_3;
    SDIO_SendCommand(&DAT_20007bbc);
    cVar1 = SD_CmdResp1Error(0x11);
    if (cVar1 == '\0') {
      SDIO_ITConfig(0x32a,1);
      SDIO_DMACmd(1);
      SD_LowLevel_DMA_RxConfig(param_1,param_5);
    }
  }
  return cVar1;
}


// ==== SD_WriteMultiBlocks @ 08023100  [SDIO]

/* CMD16, CMD55 + ACMD23 (pre-erase count), CMD25, DMA2 ch4 TX; StopCondition = 1 (CMD12 after).
   high (emulated) */

undefined8
SD_WriteMultiBlocks(undefined4 param_1,undefined4 param_2,uint param_3,int param_4,ushort param_5,
                   uint param_6)

{
  byte bVar1;
  uint uVar2;
  
  DAT_200080f7 = 0;
  DAT_20007ef4 = 0;
  DAT_20007ef0 = 1;
  SDIO.DCTRL = 0;
  if (DAT_20007ee8 == 2) {
    param_5 = 0x200;
    param_3 = param_3 >> 9 | param_4 << 0x17;
  }
  DAT_20007bbc = (uint)param_5;
  DAT_20007bc0 = 0x10;
  DAT_20007bc4 = 0x40;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  SDIO_SendCommand(&DAT_20007bbc);
  bVar1 = SD_CmdResp1Error(0x10);
  if (bVar1 == 0) {
    DAT_20007bbc = DAT_20007eec << 0x10;
    DAT_20007bc0 = 0x37;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    SDIO_SendCommand(&DAT_20007bbc);
    bVar1 = SD_CmdResp1Error(0x37);
    if (bVar1 == 0) {
      DAT_20007bbc = param_6;
      DAT_20007bc0 = 0x17;
      DAT_20007bc4 = 0x40;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      bVar1 = SD_CmdResp1Error(0x17);
      if (bVar1 == 0) {
        DAT_20007bc0 = 0x19;
        DAT_20007bc4 = 0x40;
        DAT_20007bc8 = 0;
        DAT_20007bcc = 0x400;
        DAT_20007bbc = param_3;
        SDIO_SendCommand(&DAT_20007bbc);
        bVar1 = SD_CmdResp1Error(0x19);
        if (bVar1 == 0) {
          DAT_20007b68 = 0xffffffff;
          DAT_20007b6c = param_5 * param_6;
          DAT_20007b70 = 0x90;
          DAT_20007b74 = 0;
          DAT_20007b78 = 0;
          DAT_20007b7c = 1;
          SDIO_DataConfig(&DAT_20007b68);
          SDIO_ITConfig(0x32a,1);
          SDIO_DMACmd(1);
          SD_LowLevel_DMA_TxConfig(param_1,param_5 * param_6);
          uVar2 = 0;
        }
        else {
          uVar2 = (uint)bVar1;
        }
      }
      else {
        uVar2 = (uint)bVar1;
      }
    }
    else {
      uVar2 = (uint)bVar1;
    }
  }
  else {
    uVar2 = (uint)bVar1;
  }
  return CONCAT44(param_4,uVar2);
}


// ==== SD_WaitWriteOperation @ 080232e0  [SDIO]

/* WARNING: Removing unreachable block (ram,0x08023394) */
/* WARNING: Removing unreachable block (ram,0x0802339a) */
/* waits DMAEndOfTransfer / TransferEnd / TransferError (~1000 polls only), then TXACT clear, then
   CMD12. high (emulated) */

undefined8 SD_WaitWriteOperation(void)

{
  dword dVar1;
  undefined4 in_r3;
  uint uVar2;
  
  uVar2 = 0;
  DAT_20007efc = 1000;
  do {
    if ((((DAT_20007ef8 != 0) || (DAT_20007ef4 != 0)) || (DAT_200080f7 != 0)) || (DAT_20007efc == 0)
       ) {
      DAT_20007ef8 = 0;
      DAT_20007efc = 1000;
      goto LAB_08023350;
    }
    DAT_20007efc = DAT_20007efc + -1;
  } while (DAT_20007efc != 0);
  uVar2 = 4;
  DAT_20007efc = 0;
  goto LAB_080233bc;
  while (DAT_20007efc = DAT_20007efc + -1, DAT_20007efc != 0) {
LAB_08023350:
    dVar1 = SDIO.STA;
    if ((-1 < (int)(dVar1 << 0x13)) || (DAT_20007efc == 0)) {
      if (DAT_20007ef0 == 1) {
        uVar2 = SD_StopTransfer();
      }
      SDIO_ClearFlag(0x5ff);
      if (DAT_200080f7 == 0) {
        uVar2 = uVar2 & 0xff;
      }
      else {
        uVar2 = (uint)DAT_200080f7;
      }
      goto LAB_080233bc;
    }
  }
  uVar2 = 4;
LAB_080233bc:
  return CONCAT44(in_r3,uVar2);
}


// ==== SD_StopTransfer @ 080233be

/* CMD12. high */

undefined1 SD_StopTransfer(void)

{
  undefined1 uVar1;
  
  DAT_20007bbc = 0;
  DAT_20007bc0 = 0xc;
  DAT_20007bc4 = 0x40;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  SDIO_SendCommand(&DAT_20007bbc);
  uVar1 = SD_CmdResp1Error(0xc);
  return uVar1;
}


// ==== SD_SendStatus @ 08023408

/* CMD13 with RCA; status word to *param_1 (0x26 = invalid parameter if NULL). high */

undefined8
SD_SendStatus(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  uint uVar2;
  undefined4 uVar3;
  
  if (param_1 == (undefined4 *)0x0) {
    uVar2 = 0x26;
  }
  else {
    DAT_20007bbc = DAT_20007eec << 0x10;
    DAT_20007bc0 = 0xd;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    SDIO_SendCommand(&DAT_20007bbc);
    bVar1 = SD_CmdResp1Error(0xd);
    if (bVar1 == 0) {
      uVar3 = SDIO_GetResponse(0);
      *param_1 = uVar3;
      uVar2 = 0;
    }
    else {
      uVar2 = (uint)bVar1;
    }
  }
  return CONCAT44(param_4,uVar2);
}


// ==== SD_ProcessIRQSrc @ 08023476

/* ST sdio_sd.c: from SDIO_IRQHandler; DATAEND -> TransferError 0 (0x200080f7) + TransferEnd
   0x20007ef4; DCRCFAIL 2, DTIMEOUT 4, RXOVERR 6, TXUNDERR 5, STBITERR 7; then masks the SDIO
   interrupts (0xC33A). high */

undefined8 SD_ProcessIRQSrc(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = SDIO_GetITStatus(0x100);
  if (iVar1 == 0) {
    iVar1 = SDIO_GetITStatus(2);
    if (iVar1 == 0) {
      iVar1 = SDIO_GetITStatus(8);
      if (iVar1 == 0) {
        iVar1 = SDIO_GetITStatus(0x20);
        if (iVar1 == 0) {
          iVar1 = SDIO_GetITStatus(0x10);
          if (iVar1 == 0) {
            iVar1 = SDIO_GetITStatus(0x200);
            if (iVar1 != 0) {
              SDIO_ClearITPendingBit(0x200);
              DAT_200080f7 = 7;
            }
          }
          else {
            SDIO_ClearITPendingBit(0x10);
            DAT_200080f7 = 5;
          }
        }
        else {
          SDIO_ClearITPendingBit(0x20);
          DAT_200080f7 = 6;
        }
      }
      else {
        SDIO_ClearITPendingBit(8);
        DAT_200080f7 = 4;
      }
    }
    else {
      SDIO_ClearITPendingBit(2);
      DAT_200080f7 = 2;
    }
  }
  else {
    DAT_200080f7 = 0;
    SDIO_ClearITPendingBit(0x100);
    DAT_20007ef4 = 1;
  }
  SDIO_ITConfig(0xc33a,0);
  return CONCAT44(unaff_r7,(uint)DAT_200080f7);
}


// ==== SD_ProcessDMAIRQ @ 08023524  [DMA2]

/* ST sdio_sd.c: DMA2 ISR TC4/... (0x10002000) -> DMAEndOfTransfer 0x20007ef8 = 1, DMA_ClearFlag(TC4
   | TE4); from DMA2_Channel4_IRQHandler. high */

undefined4 SD_ProcessDMAIRQ(void)

{
  dword dVar1;
  undefined4 unaff_r7;
  
  dVar1 = DMA2.ISR;
  if ((dVar1 & 0x10002000) != 0) {
    DAT_20007ef8 = 1;
    DMA_ClearFlag(0x1000f000);
  }
  return unaff_r7;
}


// ==== SD_CmdError @ 08023558

/* waits CMDSENT (CMD0, no response). high */

undefined4 SD_CmdError(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = 0x10000;
  iVar3 = 20000;
  while ((iVar2 != 0 && (iVar1 = SDIO_GetFlagStatus(0x80), iVar1 == 0))) {
    iVar2 = iVar2 + -1;
    iVar3 = iVar3 + -1;
    if (iVar3 == 0) {
      return 3;
    }
  }
  if (iVar2 == 0) {
    return 3;
  }
  SDIO_ClearFlag(0x5ff);
  return 0;
}


// ==== SD_CmdResp7Error @ 080235a4  [SDIO]

/* CMD8: CMDREND = SD 2.0 card, timeout = 1.x card. high */

undefined8 SD_CmdResp7Error(void)

{
  undefined4 uVar1;
  undefined4 in_r3;
  dword dVar2;
  int iVar3;
  int iVar4;
  
  iVar4 = 0x10000;
  dVar2 = SDIO.STA;
  iVar3 = 20000;
  while (((dVar2 & 0x45) == 0 && (iVar4 != 0))) {
    iVar4 = iVar4 + -1;
    dVar2 = SDIO.STA;
    iVar3 = iVar3 + -1;
    if (iVar3 == 0) {
      SDIO_ClearFlag(4);
      uVar1 = 3;
LAB_08023614:
      return CONCAT44(in_r3,uVar1);
    }
  }
  if ((iVar4 == 0) || ((int)(dVar2 << 0x1d) < 0)) {
    SDIO_ClearFlag(4);
    uVar1 = 3;
  }
  else if ((int)(dVar2 << 0x19) < 0) {
    SDIO_ClearFlag(0x40);
    uVar1 = 0;
  }
  else {
    uVar1 = 0;
  }
  goto LAB_08023614;
}


// ==== SD_CmdResp1Error @ 0802361c  [SDIO]

/* waits CCRCFAIL/CTIMEOUT/CMDREND, RESPCMD == command, card status & 0xFDFFE008 == 0, else error
   per bit. high */

undefined4 SD_CmdResp1Error(byte param_1)

{
  uint uVar1;
  dword dVar2;
  int iVar3;
  
  dVar2 = SDIO.STA;
  iVar3 = 20000;
  while ((dVar2 & 0x45) == 0) {
    dVar2 = SDIO.STA;
    iVar3 = iVar3 + -1;
    if (iVar3 == 0) {
      SDIO_ClearFlag(4);
      return 3;
    }
  }
  if ((int)(dVar2 << 0x1d) < 0) {
    SDIO_ClearFlag(4);
    return 3;
  }
  if ((int)(dVar2 << 0x1f) < 0) {
    SDIO_ClearFlag(1);
    return 1;
  }
  uVar1 = SDIO_GetCommandResponse();
  if (uVar1 != param_1) {
    return 0x10;
  }
  SDIO_ClearFlag(0x5ff);
  uVar1 = SDIO_GetResponse(0);
  if ((uVar1 & 0xfdffe008) == 0) {
    return 0;
  }
  if ((int)uVar1 < 0) {
    return 0x1c;
  }
  if ((int)(uVar1 << 1) < 0) {
    return 9;
  }
  if ((int)(uVar1 << 2) < 0) {
    return 10;
  }
  if ((int)(uVar1 << 3) < 0) {
    return 0xb;
  }
  if ((int)(uVar1 << 4) < 0) {
    return 0xc;
  }
  if ((int)(uVar1 << 5) < 0) {
    return 0xd;
  }
  if ((int)(uVar1 << 7) < 0) {
    return 0xe;
  }
  if ((int)(uVar1 << 8) < 0) {
    return 0xf;
  }
  if ((int)(uVar1 << 9) < 0) {
    return 0x10;
  }
  if ((int)(uVar1 << 10) < 0) {
    return 0x11;
  }
  if ((int)(uVar1 << 0xb) < 0) {
    return 0x12;
  }
  if ((int)(uVar1 << 0xc) < 0) {
    return 0x13;
  }
  if ((int)(uVar1 << 0xd) < 0) {
    return 0x14;
  }
  if ((int)(uVar1 << 0xe) < 0) {
    return 0x15;
  }
  if ((int)(uVar1 << 0xf) < 0) {
    return 0x16;
  }
  if ((int)(uVar1 << 0x10) < 0) {
    return 0x17;
  }
  if ((int)(uVar1 << 0x11) < 0) {
    return 0x18;
  }
  if ((int)(uVar1 << 0x12) < 0) {
    return 0x19;
  }
  if ((int)(uVar1 << 0x1c) < 0) {
    return 0x1a;
  }
  return 0;
}


// ==== SD_CmdResp3Error @ 08023752  [SDIO]

/* ACMD41 (no CRC check). high */

undefined4 SD_CmdResp3Error(void)

{
  dword dVar1;
  int iVar2;
  
  dVar1 = SDIO.STA;
  iVar2 = 20000;
  do {
    if ((dVar1 & 0x45) != 0) {
      if ((int)(dVar1 << 0x1d) < 0) {
        SDIO_ClearFlag(4);
        return 3;
      }
      SDIO_ClearFlag(0x5ff);
      return 0;
    }
    dVar1 = SDIO.STA;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  SDIO_ClearFlag(4);
  return 3;
}


// ==== SD_CmdResp2Error @ 080237aa  [SDIO]

/* CMD2/CMD9 (long response). high */

undefined4 SD_CmdResp2Error(void)

{
  dword dVar1;
  int iVar2;
  
  dVar1 = SDIO.STA;
  iVar2 = 20000;
  while ((dVar1 & 0x45) == 0) {
    dVar1 = SDIO.STA;
    iVar2 = iVar2 + -1;
    if (iVar2 == 0) {
      SDIO_ClearFlag(4);
      return 3;
    }
  }
  if ((int)(dVar1 << 0x1d) < 0) {
    SDIO_ClearFlag(4);
    return 3;
  }
  if ((int)(dVar1 << 0x1f) < 0) {
    SDIO_ClearFlag(1);
    return 1;
  }
  SDIO_ClearFlag(0x5ff);
  return 0;
}


// ==== SD_CmdResp6Error @ 0802381c  [SDIO]

/* CMD3: RCA from RESP1 >> 16. high */

undefined8 SD_CmdResp6Error(byte param_1,undefined2 *param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uVar1;
  uint uVar2;
  dword dVar3;
  int iVar4;
  
  dVar3 = SDIO.STA;
  iVar4 = 20000;
  do {
    if ((dVar3 & 0x45) != 0) {
      if ((int)(dVar3 << 0x1d) < 0) {
        SDIO_ClearFlag(4);
        uVar1 = 3;
      }
      else if ((int)(dVar3 << 0x1f) < 0) {
        SDIO_ClearFlag(1);
        uVar1 = 1;
      }
      else {
        uVar2 = SDIO_GetCommandResponse();
        if (uVar2 == param_1) {
          SDIO_ClearFlag(0x5ff);
          uVar2 = SDIO_GetResponse(0);
          if ((uVar2 & 0xe000) == 0) {
            *param_2 = (short)(uVar2 >> 0x10);
            uVar1 = 0;
          }
          else if ((int)(uVar2 << 0x12) < 0) {
            uVar1 = 0x13;
          }
          else if ((int)(uVar2 << 0x11) < 0) {
            uVar1 = 0x10;
          }
          else if ((int)(uVar2 << 0x10) < 0) {
            uVar1 = 0xf;
          }
          else {
            uVar1 = 0;
          }
        }
        else {
          uVar1 = 0x10;
        }
      }
      goto LAB_080238de;
    }
    dVar3 = SDIO.STA;
    iVar4 = iVar4 + -1;
  } while (iVar4 != 0);
  SDIO_ClearFlag(4);
  uVar1 = 3;
LAB_080238de:
  return CONCAT44(param_4,uVar1);
}


// ==== SDEnWideBus @ 080238f0

/* ST sdio_sd.c static: card locked -> 0xE (SD_LOCK_UNLOCK_FAILED); SD_FindSCR; arg 1 needs SCR
   4-bit support, ACMD6 arg 2; arg 0 needs 1-bit support, ACMD6 arg 0; caller
   SD_EnableWideBusOperation (08022ece) and SD_Init passes 1-bit. high */

undefined8 SDEnWideBus(char param_1)

{
  byte bVar1;
  int iVar2;
  uint uVar3;
  undefined4 local_18;
  int local_14;
  
  local_18 = 0;
  local_14 = 0;
  iVar2 = SDIO_GetResponse(0);
  if (iVar2 << 6 < 0) {
    uVar3 = 0xe;
  }
  else {
    bVar1 = SD_FindSCR(DAT_20007eec & 0xffff,&local_18);
    if (bVar1 == 0) {
      if (param_1 == '\x01') {
        if (local_14 << 0xd < 0) {
          DAT_20007bbc = DAT_20007eec << 0x10;
          DAT_20007bc0 = 0x37;
          DAT_20007bc4 = 0x40;
          DAT_20007bc8 = 0;
          DAT_20007bcc = 0x400;
          SDIO_SendCommand(&DAT_20007bbc);
          bVar1 = SD_CmdResp1Error(0x37);
          if (bVar1 == 0) {
            DAT_20007bbc = 2;
            DAT_20007bc0 = 6;
            DAT_20007bc4 = 0x40;
            DAT_20007bc8 = 0;
            DAT_20007bcc = 0x400;
            SDIO_SendCommand(&DAT_20007bbc);
            bVar1 = SD_CmdResp1Error(6);
            if (bVar1 == 0) {
              uVar3 = 0;
            }
            else {
              uVar3 = (uint)bVar1;
            }
          }
          else {
            uVar3 = (uint)bVar1;
          }
        }
        else {
          uVar3 = 0x25;
        }
      }
      else if (local_14 << 0xf < 0) {
        DAT_20007bbc = DAT_20007eec << 0x10;
        DAT_20007bc0 = 0x37;
        DAT_20007bc4 = 0x40;
        DAT_20007bc8 = 0;
        DAT_20007bcc = 0x400;
        SDIO_SendCommand(&DAT_20007bbc);
        bVar1 = SD_CmdResp1Error(0x37);
        if (bVar1 == 0) {
          DAT_20007bbc = 0;
          DAT_20007bc0 = 6;
          DAT_20007bc4 = 0x40;
          DAT_20007bc8 = 0;
          DAT_20007bcc = 0x400;
          SDIO_SendCommand(&DAT_20007bbc);
          bVar1 = SD_CmdResp1Error(6);
          if (bVar1 == 0) {
            uVar3 = 0;
          }
          else {
            uVar3 = (uint)bVar1;
          }
        }
        else {
          uVar3 = (uint)bVar1;
        }
      }
      else {
        uVar3 = 0x25;
      }
    }
    else {
      uVar3 = (uint)bVar1;
    }
  }
  return CONCAT44(local_18,uVar3);
}


// ==== SD_FindSCR @ 08023a8c  [SDIO]

/* CMD16 (8), CMD55, ACMD51, FIFO polling read of the 8-byte SCR. high (emulated) */

undefined8 SD_FindSCR(undefined4 param_1,uint *param_2)

{
  dword dVar1;
  byte bVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  uint local_20 [2];
  
  iVar5 = 0;
  local_20[0] = 0;
  local_20[1] = 0;
  DAT_20007bbc = 8;
  DAT_20007bc0 = 0x10;
  DAT_20007bc4 = 0x40;
  DAT_20007bc8 = 0;
  DAT_20007bcc = 0x400;
  SDIO_SendCommand(&DAT_20007bbc);
  bVar2 = SD_CmdResp1Error(0x10);
  if (bVar2 == 0) {
    DAT_20007bbc = DAT_20007eec << 0x10;
    DAT_20007bc0 = 0x37;
    DAT_20007bc4 = 0x40;
    DAT_20007bc8 = 0;
    DAT_20007bcc = 0x400;
    SDIO_SendCommand(&DAT_20007bbc);
    bVar2 = SD_CmdResp1Error(0x37);
    if (bVar2 == 0) {
      DAT_20007b68 = 0xffffffff;
      DAT_20007b6c = 8;
      DAT_20007b70 = 0x30;
      DAT_20007b74 = 2;
      DAT_20007b78 = 0;
      DAT_20007b7c = 1;
      SDIO_DataConfig(&DAT_20007b68);
      DAT_20007bbc = 0;
      DAT_20007bc0 = 0x33;
      DAT_20007bc4 = 0x40;
      DAT_20007bc8 = 0;
      DAT_20007bcc = 0x400;
      SDIO_SendCommand(&DAT_20007bbc);
      bVar2 = SD_CmdResp1Error(0x33);
      if (bVar2 == 0) {
        iVar6 = 20000;
        do {
          dVar1 = SDIO.STA;
          if ((dVar1 & 0x62a) != 0) {
            iVar5 = SDIO_GetFlagStatus(8);
            if (iVar5 == 0) {
              iVar5 = SDIO_GetFlagStatus(2);
              if (iVar5 == 0) {
                iVar5 = SDIO_GetFlagStatus(0x20);
                if (iVar5 == 0) {
                  iVar5 = SDIO_GetFlagStatus(0x200);
                  if (iVar5 == 0) {
                    SDIO_ClearFlag(0x5ff);
                    param_2[1] = (local_20[0] & 0xff00) << 8 | local_20[0] << 0x18 |
                                 local_20[0] >> 8 & 0xff00 | local_20[0] >> 0x18;
                    *param_2 = (local_20[1] & 0xff00) << 8 | local_20[1] << 0x18 |
                               local_20[1] >> 8 & 0xff00 | local_20[1] >> 0x18;
                    uVar3 = 0;
                  }
                  else {
                    SDIO_ClearFlag(0x200);
                    uVar3 = 7;
                  }
                }
                else {
                  SDIO_ClearFlag(0x20);
                  uVar3 = 6;
                }
              }
              else {
                SDIO_ClearFlag(2);
                uVar3 = 2;
              }
            }
            else {
              SDIO_ClearFlag(8);
              uVar3 = 4;
            }
            goto LAB_08023c7c;
          }
          iVar4 = SDIO_GetFlagStatus(0x200000);
          if (iVar4 != 0) {
            uVar3 = SDIO_ReadData();
            local_20[iVar5] = uVar3;
            iVar5 = iVar5 + 1;
          }
          iVar6 = iVar6 + -1;
        } while (iVar6 != 0);
        SDIO_ClearFlag(8);
        uVar3 = 4;
      }
      else {
        uVar3 = (uint)bVar2;
      }
    }
    else {
      uVar3 = (uint)bVar2;
    }
  }
  else {
    uVar3 = (uint)bVar2;
  }
LAB_08023c7c:
  return CONCAT44(local_20[0],uVar3);
}


// ==== strlen @ 08023ca0

/* library (IAR, word-at-a-time scan); Log_FormatRow, Log_PutTime. high */

int strlen(uint *param_1)

{
  char cVar1;
  uint *puVar2;
  uint *puVar3;
  uint uVar4;
  
  puVar3 = param_1;
  while (((uint)puVar3 & 3) != 0) {
    puVar2 = (uint *)((int)puVar3 + 1);
    uVar4 = *puVar3;
    puVar3 = puVar2;
    if ((char)uVar4 == '\0') goto LAB_08023cd2;
  }
  uVar4 = *puVar3;
  while ((uVar4 + 0xfefefeff & ~uVar4 & 0x80808080) == 0) {
    puVar3 = puVar3 + 1;
    uVar4 = *puVar3;
  }
  cVar1 = (char)*puVar3;
  puVar2 = puVar3;
  while (puVar2 = (uint *)((int)puVar2 + 1), cVar1 != '\0') {
    cVar1 = *(char *)puVar2;
  }
LAB_08023cd2:
  return (int)puVar2 - (int)((int)param_1 + 1);
}


// ==== SD_LowLevel_DeInit @ 08023cd8  [GPIOC GPIOD]

/* ST sdio_sd.c (board part): SDIO clock off, power off, SDIO_DeInit, SDIO APB2 clock off,
   PC8/PC12/PD2 back to AF0 and input mode. high */

undefined8 SD_LowLevel_DeInit(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined2 uStack_b;
  undefined1 local_9;
  
  SDIO_ClockCmd(0);
  SDIO_SetPowerState(0);
  SDIO_DeInit();
  RCC_APB2PeriphClockCmd(0x800,0);
  GPIO_PinAFConfig(&GPIOC,8,0);
  GPIO_PinAFConfig(&GPIOC,0xc,0);
  GPIO_PinAFConfig(&GPIOD,2,0);
  local_10 = 0x100;
  local_c = 0;
  local_9 = 0;
  GPIO_Init(&GPIOC,&local_10);
  local_10 = 4;
  GPIO_Init(&GPIOD,&local_10);
  local_10 = 0x1000;
  GPIO_Init(&GPIOC,&local_10);
  return CONCAT17(local_9,CONCAT25(uStack_b,CONCAT14(local_c,local_10)));
}


// ==== SD_LowLevel_Init @ 08023d56  [GPIOC GPIOD]

/* ST sdio_sd.c (board part): GPIOC/D clocks, PC8 (D0) / PD2 (CMD) / PC12 (CLK) AF12, 40 MHz,
   pull-up on D0/CMD only; SDIO (APB2 0x800) and DMA2 (AHB) clocks on. high */

undefined8 SD_LowLevel_Init(void)

{
  undefined4 local_10;
  undefined1 local_c;
  undefined1 local_b;
  undefined1 local_a;
  undefined1 local_9;
  
  RCC_AHBPeriphClockCmd(0xc,1);
  GPIO_PinAFConfig(&GPIOC,8,0xc);
  GPIO_PinAFConfig(&GPIOC,0xc,0xc);
  GPIO_PinAFConfig(&GPIOD,2,0xc);
  local_10 = 0x100;
  local_b = 3;
  local_c = 2;
  local_a = 0;
  local_9 = 1;
  GPIO_Init(&GPIOC,&local_10);
  local_10 = 4;
  GPIO_Init(&GPIOD,&local_10);
  local_10 = 0x1000;
  local_9 = 0;
  GPIO_Init(&GPIOC,&local_10);
  RCC_APB2PeriphClockCmd(0x800,1);
  RCC_AHBPeriphClockCmd(0x2000000,1);
  return CONCAT17(local_9,CONCAT16(local_a,CONCAT15(local_b,CONCAT14(local_c,local_10))));
}


// ==== SD_LowLevel_DMA_TxConfig @ 08023de8  [DMA2 SDIO]

/* DMA2 channel 4, memory to SDIO.FIFO, words, TC interrupt. high */

void SD_LowLevel_DMA_TxConfig(undefined4 param_1,uint param_2)

{
  dword *local_38;
  undefined4 local_34;
  undefined4 local_30;
  uint local_2c;
  undefined4 local_28;
  undefined4 local_24;
  undefined4 local_20;
  undefined4 local_1c;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  
  DMA_ClearFlag(0x1000f000);
  DMA_Cmd(&DMA2.CCR4,0);
  local_38 = &SDIO.FIFO;
  local_30 = 0x10;
  local_2c = param_2 >> 2;
  local_28 = 0;
  local_24 = 0x80;
  local_20 = 0x200;
  local_1c = 0x800;
  local_18 = 0;
  local_14 = 0x2000;
  local_10 = 0;
  local_34 = param_1;
  DMA_Init(&DMA2.CCR4,&local_38);
  DMA_ITConfig(&DMA2.CCR4,2,1);
  DMA_Cmd(&DMA2.CCR4,1);
  return;
}


// ==== SD_LowLevel_DMA_RxConfig @ 08023e58  [DMA2 SDIO]

/* DMA2 channel 4, SDIO.FIFO to memory. high */

void SD_LowLevel_DMA_RxConfig(undefined4 param_1,uint param_2)

{
  dword *local_38;
  undefined4 local_34;
  undefined4 local_30;
  uint local_2c;
  undefined4 local_28;
  undefined4 local_24;
  undefined4 local_20;
  undefined4 local_1c;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  
  DMA_ClearFlag(0x1000f000);
  DMA_Cmd(&DMA2.CCR4,0);
  local_38 = &SDIO.FIFO;
  local_30 = 0;
  local_2c = param_2 >> 2;
  local_28 = 0;
  local_24 = 0x80;
  local_20 = 0x200;
  local_1c = 0x800;
  local_18 = 0;
  local_14 = 0x2000;
  local_10 = 0;
  local_34 = param_1;
  DMA_Init(&DMA2.CCR4,&local_38);
  DMA_ITConfig(&DMA2.CCR4,2,1);
  DMA_Cmd(&DMA2.CCR4,1);
  return;
}


// ==== __aeabi_errno_addr @ 08023edc

/* IAR runtime: returns &errno (0x20000228); sqrt / log10 set EDOM / ERANGE through it. library */

undefined * __aeabi_errno_addr(void)

{
  return &DAT_20000228;
}


// ==== _Dnorm @ 08023ee8

/* library (IAR Dinkumware): normalizes a subnormal double in place (shift mantissa until bit 52,
   keep sign), returns 1 - shift count; zero returns 1; log10 calls it when the exponent field is 0.
   medium */

int _Dnorm(uint *param_1)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  bool bVar4;
  
  uVar2 = param_1[1];
  iVar1 = 1;
  uVar3 = uVar2 & 0xfffff;
  param_1[1] = uVar3;
  bVar4 = uVar3 != 0;
  if (!bVar4) {
    uVar3 = *param_1;
  }
  if (bVar4 || uVar3 != 0) {
    while (uVar3 = param_1[1], -1 < (int)(uVar3 << 0xb)) {
      param_1[1] = *param_1 >> 0x1f | uVar3 << 1;
      *param_1 = *param_1 << 1;
      iVar1 = iVar1 + -1;
    }
    param_1[1] = uVar3 & 0xfffff;
  }
  param_1[1] = uVar2 & 0x80000000 | param_1[1];
  return iVar1;
}


// ==== __aeabi_cdcmple @ 08023f2c

/* IAR runtime double compare (flags of a <=> b, hi words then lo; NaN unordered); log10 vs
   sqrt(0.5). Disassembly checked. library */

void __aeabi_cdcmple(undefined4 param_1,uint param_2,undefined4 param_3,uint param_4)

{
  uint uVar1;
  
  uVar1 = param_2 * 2;
  if (uVar1 < 0xffe00001) {
    uVar1 = param_4 * 2;
  }
  if ((uVar1 < 0xffe00001) && ((int)(param_2 | param_4) < 0)) {
    return;
  }
  return;
}


// ==== disk_initialize @ 08023f5c

/* diskio glue: returns SD_Init() result as DSTATUS (drive ignored); chk_mounted. high */

ulonglong disk_initialize(void)

{
  undefined4 uVar1;
  undefined4 in_r3;
  
  uVar1 = SD_Init();
  return CONCAT44(in_r3,uVar1) & 0xffffffff000000ff;
}


// ==== disk_status @ 08023f6e

/* diskio glue: drive != 0 -> STA_NOINIT (1), else 0 (no card check); chk_mounted, validate. high */

bool disk_status(char param_1)

{
  return param_1 != '\0';
}


// ==== disk_read @ 08023f7e

/* diskio glue: ONE SD_ReadBlock(buff, sector << 9, 512) whatever count says, then polls
   SD_GetStatus 20000 times (timeout -> 2); multi-sector reads would return only the first sector
   (no caller asks: all reads are <= 512 B). high */

undefined8 disk_read(undefined4 param_1,undefined4 param_2,int param_3)

{
  int iVar1;
  undefined4 uVar2;
  int iVar3;
  
  iVar3 = 20000;
  SD_ReadBlock(param_2,param_2,param_3 << 9,0);
  do {
    iVar1 = SD_GetStatus();
    if (iVar1 == 0) {
      uVar2 = 0;
      goto LAB_08023fb4;
    }
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  uVar2 = 2;
LAB_08023fb4:
  return CONCAT44(0x200,uVar2);
}


// ==== disk_write @ 08023fb8

/* diskio glue: one sector at a time, each SD_WriteMultiBlocks(1 block, CMD25) +
   SD_WaitWriteOperation (results ignored), then polls 0802268a for transfer state 20000 times
   (timeout -> 2 = RES_WRPRT, not RES_ERROR); byte address = sector << 9 in 32 bits, high word 0, so
   sectors past 4 GiB wrap. high */

undefined8 disk_write(undefined4 param_1,int param_2,int param_3,uint param_4)

{
  int iVar1;
  undefined4 uVar2;
  uint uVar3;
  int iVar4;
  undefined4 in_stack_ffffffd0;
  
  uVar3 = 0;
  do {
    param_4 = param_4 & 0xff;
    if (param_4 <= uVar3) {
      uVar2 = 0;
LAB_0802402a:
      return CONCAT44(in_stack_ffffffd0,uVar2);
    }
    in_stack_ffffffd0 = 0x200;
    SD_WriteMultiBlocks(uVar3 * 0x200 + param_2,0x200,uVar3 * 0x200 + param_3 * 0x200,0,0x200,1);
    SD_WaitWriteOperation();
    iVar4 = 20000;
    while (iVar1 = SD_GetStatus(), iVar1 != 0) {
      iVar4 = iVar4 + -1;
      if (iVar4 == 0) {
        uVar2 = 2;
        goto LAB_0802402a;
      }
    }
    uVar3 = uVar3 + 1;
  } while( true );
}


// ==== get_fattime @ 0802402e

/* diskio glue: constant 0x3449B3C0 = 2006-02-09 22:29:00; file times come from File_SetTimestamp
   (f_utime) instead. high */

undefined4 get_fattime(void)

{
  return 0x3449b3c0;
}


// ==== disk_ioctl @ 08024038

/* diskio glue: GET_SECTOR_COUNT -> 0x20000 (fixed, 64 MB), GET_SECTOR_SIZE -> 512, GET_BLOCK_SIZE
   -> 32, CTRL_SYNC nothing; always RES_OK; only sync calls it. high */

undefined4 disk_ioctl(undefined4 param_1,byte param_2,undefined4 *param_3)

{
  if (param_2 == 1) {
    *param_3 = 0x20000;
  }
  else if (param_2 != 0) {
    if (param_2 == 3) {
      *param_3 = 0x20;
    }
    else if (param_2 < 3) {
      *(undefined2 *)param_3 = 0x200;
    }
  }
  return 0;
}


// ==== SDIO_DeInit @ 08024074

/* StdPeriph: RCC_APB2PeriphResetCmd(SDIO 0x800) on then off; SD_Init, SD_LowLevel_DeInit. library
    */

undefined4 SDIO_DeInit(void)

{
  undefined4 unaff_r7;
  
  RCC_APB2PeriphResetCmd(0x800,1);
  RCC_APB2PeriphResetCmd(0x800,0);
  return unaff_r7;
}


// ==== SDIO_Init @ 0802408c  [SDIO]

/* CLKCR fields. library */

void SDIO_Init(uint *param_1)

{
  dword dVar1;
  
  dVar1 = SDIO.CLKCR;
  SDIO.CLKCR = dVar1 & 0xffff8100 |
               (uint)(byte)param_1[5] | param_1[2] | param_1[1] | param_1[3] | *param_1 | param_1[4]
  ;
  return;
}


// ==== SDIO_ClockCmd @ 080240b6  [DAT_422580a0]

/* bit-band CLKCR.CLKEN (0x422580A0). library */

void SDIO_ClockCmd(byte param_1)

{
  DAT_422580a0 = (uint)param_1;
  return;
}


// ==== SDIO_SetPowerState @ 080240be  [SDIO]

/* SDIO.POWER. library */

void SDIO_SetPowerState(dword param_1)

{
  SDIO.POWER = param_1;
  return;
}


// ==== SDIO_GetPowerState @ 080240c4  [SDIO]

/* StdPeriph: POWER & 3; SD_InitializeCards. library */

uint SDIO_GetPowerState(void)

{
  dword dVar1;
  
  dVar1 = SDIO.POWER;
  return dVar1 & 3;
}


// ==== SDIO_DMACmd @ 080240ce  [DAT_4225858c]

/* bit-band DCTRL.DMAEN (0x4225858C). library */

void SDIO_DMACmd(byte param_1)

{
  DAT_4225858c = (uint)param_1;
  return;
}


// ==== SDIO_SendCommand @ 080240d6  [SDIO]

/* ARG, then CMD (index, response, CPSM). library */

void SDIO_SendCommand(dword *param_1)

{
  dword dVar1;
  
  SDIO.ARG = *param_1;
  dVar1 = SDIO.CMD;
  SDIO.CMD = dVar1 & 0xfffff800 | param_1[1] | param_1[2] | param_1[3] | param_1[4];
  return;
}


// ==== SDIO_GetCommandResponse @ 080240fe  [SDIO]

/* RESPCMD. library */

uint SDIO_GetCommandResponse(void)

{
  dword dVar1;
  
  dVar1 = SDIO.RESPCMD;
  return dVar1 & 0xff;
}


// ==== SDIO_GetResponse @ 08024106  [SDIO]

/* RESP1 + offset. library */

undefined4 SDIO_GetResponse(int param_1)

{
  return *(undefined4 *)((int)&SDIO.RESP1 + param_1);
}


// ==== SDIO_DataConfig @ 0802411c  [SDIO]

/* DTIMER, DLEN, DCTRL. library */

void SDIO_DataConfig(dword *param_1)

{
  dword dVar1;
  
  SDIO.DTIMER = *param_1;
  SDIO.DLEN = param_1[1];
  dVar1 = SDIO.DCTRL;
  SDIO.DCTRL = dVar1 & 0xffffff08 | param_1[2] | param_1[3] | param_1[4] | param_1[5];
  return;
}


// ==== SDIO_ReadData @ 0802414a  [SDIO]

/* StdPeriph: FIFO; SD_FindSCR polling read. library */

dword SDIO_ReadData(void)

{
  dword dVar1;
  
  dVar1 = SDIO.FIFO;
  return dVar1;
}


// ==== SDIO_ITConfig @ 08024150  [SDIO]

/* MASK set/clear. library */

void SDIO_ITConfig(uint param_1,char param_2)

{
  dword dVar1;
  
  if (param_2 == '\0') {
    dVar1 = SDIO.MASK;
    SDIO.MASK = dVar1 & ~param_1;
  }
  else {
    dVar1 = SDIO.MASK;
    SDIO.MASK = dVar1 | param_1;
  }
  return;
}


// ==== SDIO_GetFlagStatus @ 0802416e  [SDIO]

/* STA & flag. library */

bool SDIO_GetFlagStatus(uint param_1)

{
  dword dVar1;
  
  dVar1 = SDIO.STA;
  return (dVar1 & param_1) != 0;
}


// ==== SDIO_ClearFlag @ 08024188  [SDIO]

/* ICR. library */

void SDIO_ClearFlag(dword param_1)

{
  SDIO.ICR = param_1;
  return;
}


// ==== SDIO_GetITStatus @ 0802418e  [SDIO]

/* StdPeriph: STA & it (twin of SDIO_GetFlagStatus 0802416e); SD_ProcessIRQSrc. library */

bool SDIO_GetITStatus(uint param_1)

{
  dword dVar1;
  
  dVar1 = SDIO.STA;
  return (dVar1 & param_1) != 0;
}


// ==== SDIO_ClearITPendingBit @ 080241a8  [SDIO]

/* StdPeriph: ICR = it (twin of SDIO_ClearFlag 08024188); SD_ProcessIRQSrc. library */

void SDIO_ClearITPendingBit(dword param_1)

{
  SDIO.ICR = param_1;
  return;
}


// ==== DMA_Init @ 080241f0

/* StdPeriph: CCR & 0xFFFF800F | dir/inc/size/mode/prio/m2m, CNDTR, CPAR, CMAR; DMA2 ch4 SD Tx/Rx
   config. library */

void DMA_Init(uint *param_1,uint *param_2)

{
  *param_1 = *param_1 & 0xffff800f |
             param_2[2] | param_2[8] | param_2[4] | param_2[5] | param_2[6] | param_2[7] |
             param_2[9] | param_2[10];
  param_1[1] = param_2[3];
  param_1[2] = *param_2;
  param_1[3] = param_2[1];
  return;
}


// ==== DMA_Cmd @ 0802422e

/* StdPeriph: CCR EN bit; SD DMA Tx/Rx config. library */

void DMA_Cmd(uint *param_1,char param_2)

{
  if (param_2 == '\0') {
    *param_1 = *param_1 & 0xfffe;
  }
  else {
    *param_1 = *param_1 | 1;
  }
  return;
}


// ==== DMA_ITConfig @ 0802424a

/* StdPeriph: CCR |= / &= ~it (2 = TC); SD DMA Tx/Rx config. library */

void DMA_ITConfig(uint *param_1,uint param_2,char param_3)

{
  if (param_3 == '\0') {
    *param_1 = *param_1 & ~param_2;
  }
  else {
    *param_1 = *param_1 | param_2;
  }
  return;
}


// ==== DMA_ClearFlag @ 08024260  [DMA1 DMA2]

/* StdPeriph: bit 28 set -> DMA2.IFCR else DMA1.IFCR; 0x1000F000 = all DMA2 ch4 flags. library */

void DMA_ClearFlag(dword param_1)

{
  if ((int)(param_1 << 3) < 0) {
    DMA2.IFCR = param_1;
  }
  else {
    DMA1.IFCR = param_1;
  }
  return;
}


// ==== NMI_Handler @ 0802427c

/* stm32l1xx_it.c stub: returns. library */

void NMI_Handler(void)

{
  return;
}


// ==== HardFault_Handler @ 0802427e

/* stm32l1xx_it.c stub: while (1). library */

void HardFault_Handler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== MemManage_Handler @ 08024280

/* stm32l1xx_it.c stub: while (1). library */

void MemManage_Handler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== BusFault_Handler @ 08024282

/* stm32l1xx_it.c stub: while (1). library */

void BusFault_Handler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== UsageFault_Handler @ 08024284

/* stm32l1xx_it.c stub: while (1). library */

void UsageFault_Handler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== SVC_Handler @ 08024286

/* stm32l1xx_it.c stub: returns. library */

void SVC_Handler(void)

{
  return;
}


// ==== DebugMon_Handler @ 08024288

/* stm32l1xx_it.c stub: returns. library */

void DebugMon_Handler(void)

{
  return;
}


// ==== PendSV_Handler @ 0802428a

/* stm32l1xx_it.c stub: returns. library */

void PendSV_Handler(void)

{
  return;
}


// ==== TIM2_IRQHandler @ 0802428c  [TIM2 USART2]

/* 10 ms: g_tick10ms (main loop's 10 ms work), countdowns (auto-hold, mode timeouts, Hz 300-tick,
   1000-tick -> 080153ac). NVIC preempt 2 / sub 2. high */

undefined4 TIM2_IRQHandler(void)

{
  undefined4 unaff_r7;
  
  TIM_ClearFlag(0x40000000,1);
  g_tick10ms = 1;
  if (((DAT_20008035 != '\0') && (DAT_20008036 != '\0')) &&
     (DAT_20008036 = DAT_20008036 + -1, DAT_20008036 == '\0')) {
    DAT_20008035 = '\0';
  }
  if (((DAT_20007fe1 != '\0') && (g_log_interval == 0)) &&
     ((DAT_20008050 == 0 && (DAT_20007fa8 = DAT_20007fa8 + 1, 0xf < DAT_20007fa8)))) {
    DAT_20007fa8 = 0;
    DAT_20007ffa = DAT_20007ffa + 1;
    if (DAT_20007ffa < 6) {
      DAT_20007fe2 = 1;
    }
  }
  if ((DAT_20007fe4 != '\0') && (DAT_20007faa = DAT_20007faa + 1, 1000 < DAT_20007faa)) {
    Display_ResetToLive();
  }
  if (DAT_200080b4 != '\0') {
    DAT_200080b4 = DAT_200080b4 + -1;
  }
  if ((((((((g_mode == '\x10') || (g_mode == '\x12')) || (g_mode == '\x14')) ||
         ((g_mode == '\x11' || (g_mode == '\x13')))) || (g_mode == '\x15')) &&
       (g_setup_page == '\n')) || (g_setup_page == '\f')) &&
     ((DAT_200080aa != '\0' && (DAT_200080aa = DAT_200080aa + -1, DAT_200080aa == '\0')))) {
    DAT_20008062 = 1;
  }
  if (((g_mode == '\f') && (DAT_20007fc4 != 0)) &&
     (DAT_20007fc4 = DAT_20007fc4 + -1, DAT_20007fc4 == 0)) {
    DAT_200080a3 = 1;
  }
  DAT_20007fc6 = DAT_20007fc6 + 1;
  if ((((g_mode == '\x06') || (g_mode == '\a')) || (g_mode == '\b')) &&
     (DAT_20007fc0 = DAT_20007fc0 + 1, 300 < DAT_20007fc0)) {
    DAT_20007fc0 = 0;
    g_reading = 0;
    if (DAT_20008042 == '\0') {
      g_range = 0;
    }
    if (DAT_20008050 == 0) {
      g_display_update = 1;
      if (DAT_20008042 == '\0') {
        DAT_2000804b = 1;
      }
    }
    else {
      DAT_20008052 = 1;
      DAT_20008051 = 0;
      DAT_20007fb2 = 0;
    }
  }
  if ((((g_mode == '\x10') || (g_mode == '\x12')) ||
      ((g_mode == '\x14' || ((g_mode == '\x02' || (g_mode == '\x04')))))) &&
     (DAT_20007fc0 = DAT_20007fc0 + 1, 2000 < DAT_20007fc0)) {
    DAT_20007fc0 = 0;
    DAT_2000803e = 0;
    DAT_20008062 = 1;
    DAT_20007d84 = 0;
  }
  if (((g_mode == '\0') && (DAT_20008098 != '\0')) &&
     (DAT_20007fc0 = DAT_20007fc0 + 1, 500 < DAT_20007fc0)) {
    DAT_20007fc0 = 0;
    DAT_2000803e = 0;
    DAT_20008062 = 1;
    DAT_20007d84 = 0;
  }
  if (((DAT_20008050 & (DAT_20008052 ^ 1)) != 0) &&
     (DAT_20007fb2 = DAT_20007fb2 + 1, DAT_20007fb2 == 0x32)) {
    DAT_20007fb2 = 0;
    DAT_20008051 = 1;
  }
  if (0x31 < DAT_20007fb2) {
    DAT_20007fb2 = 0;
  }
  DAT_20007f00 = DAT_20007f00 + 1;
  if (9 < DAT_20007f00) {
    DAT_20007f00 = 0;
    DAT_200080d1 = 1;
  }
  if (DAT_20007f04 == 0x28) {
    DAT_2000802c = 1;
  }
  DAT_20007f04 = DAT_20007f04 + 1;
  if (0x31 < DAT_20007f04) {
    DAT_20007f04 = 0;
    DAT_200080d3 = 1;
    if (g_ble_on != '\0') {
      DAT_20008031 = 0;
      USART_SendByteBlocking(&USART2,g_ble_tx);
      USART_ITConfig(&USART2,0x727,1);
    }
    DAT_20008077 = DAT_20008077 != '\x01';
    if ((DAT_20008076 != '\0' || DAT_200080b2 != '\0') && ((bool)DAT_20008077)) {
      Buzzer_OnAlways(500);
    }
    if (DAT_2000806e != '\0') {
      DAT_2000806f = DAT_2000806f != '\x01';
      DAT_20008062 = 1;
    }
    if (g_setup_page == '\a') {
      DAT_200080d5 = DAT_200080d5 != '\x01';
      LCD_SubHourMinuteBlink(1);
    }
  }
  DAT_20007f08 = DAT_20007f08 + 1;
  if (99 < DAT_20007f08) {
    DAT_20007f08 = 0;
  }
  return unaff_r7;
}


// ==== TIM3_IRQHandler @ 08024722  [GPIOE TIM3]

/* 1 ms: g_tick1ms, g_ms_countdown--, buzzer PE7 on/off pattern. NVIC preempt 1 / sub 1. high */

undefined4 TIM3_IRQHandler(void)

{
  undefined4 unaff_r7;
  
  TIM_ClearFlag(&TIM3,1);
  g_tick1ms = 1;
  if (g_ms_countdown != 0) {
    g_ms_countdown = g_ms_countdown + -1;
  }
  if (((DAT_20007fb4 != 0) && (DAT_2000803a == '\0')) &&
     (DAT_20007fb4 = DAT_20007fb4 + -1, DAT_20007fb4 == 0)) {
    DAT_20008079 = 0;
    GPIOE.BSRR._2_2_ = 0x80;
    if (DAT_2000807a != 0) {
      DAT_20007fb4 = DAT_20007fb6;
      DAT_2000807a = DAT_2000807a - 1;
      if (((uint)DAT_2000807a % 2 == 1) && (DAT_20008079 = 1, g_buzzer_enabled != '\0')) {
        GPIOE.BSRR._0_2_ = 0x80;
      }
    }
  }
  return unaff_r7;
}


// ==== USART2_IRQHandler @ 080247d8  [USART2]

/* BLE: TXE sends g_ble_tx (19 B, one byte per IRQ); RXNE collects g_ble_rx: 0xF4 = 5-byte key
   frame, 0xF8 = 15-byte set-clock frame -> 0x20008030 ready. Not enabled in Board_NVICInit (later).
   high */

undefined4 USART2_IRQHandler(void)

{
  undefined1 uVar1;
  int iVar2;
  undefined4 unaff_r7;
  
  iVar2 = USART_GetITStatus(&USART2,0x727);
  if (iVar2 != 0) {
    USART_ClearITPendingBit(&USART2,0x727);
    DAT_20008031 = DAT_20008031 + 1;
    if (DAT_20008031 < 0x13) {
      USART_SendByteBlocking(&USART2,(&g_ble_tx)[DAT_20008031]);
    }
    else {
      USART_ITConfig(&USART2,0x727,0);
    }
  }
  iVar2 = USART_GetITStatus(&USART2,0x525);
  if (iVar2 != 0) {
    uVar1 = USART_ReceiveData(&USART2);
    (&g_ble_rx)[DAT_2000802d] = uVar1;
    USART_ClearITPendingBit(&USART2,0x525);
    DAT_2000802d = DAT_2000802d + 1;
    if (0x13 < DAT_2000802d) {
      DAT_2000802d = 0;
    }
    if (DAT_2000802d == 1) {
      if (g_ble_rx == -0xc) {
        DAT_2000802e = 5;
      }
      else if (g_ble_rx == -8) {
        DAT_2000802e = 0xf;
      }
      else {
        DAT_2000802d = 0;
      }
    }
    else if (DAT_2000802e <= DAT_2000802d) {
      DAT_20008030 = 1;
      DAT_2000802f = DAT_2000802e;
      DAT_2000802d = 0;
    }
  }
  return unaff_r7;
}


// ==== SysTick_Handler @ 080248b0

/* calls 0800ed0c. high */

undefined4 SysTick_Handler(void)

{
  undefined4 unaff_r7;
  
  SysTick_Work();
  return unaff_r7;
}


// ==== SDIO_IRQHandler @ 080248b8

/* SD driver 08023476. NVIC preempt 0. high */

undefined4 SDIO_IRQHandler(void)

{
  undefined4 unaff_r7;
  
  SD_ProcessIRQSrc();
  return unaff_r7;
}


// ==== DMA2_Channel4_IRQHandler @ 080248c0

/* SD DMA end of transfer (vector 53). high */

undefined4 DMA2_Channel4_IRQHandler(void)

{
  undefined4 unaff_r7;
  
  SD_ProcessDMAIRQ();
  return unaff_r7;
}


// ==== RTC_WKUP_IRQHandler @ 080248c8

/* vector name kept: RTC_IT_WUT pending -> RTC_ClearITPendingBit + EXTI line 20 clear; no other
   work. high */

undefined4 RTC_WKUP_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = RTC_GetITStatus(0x4000);
  if (iVar1 != 0) {
    RTC_ClearITPendingBit(0x4000);
    EXTI_ClearITPendingBit(0x100000);
  }
  return unaff_r7;
}


// ==== EXTI0_IRQHandler @ 080248e8

/* vector name kept: only clears pending EXTI line 0 (wake from STOP). high */

undefined4 EXTI0_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(1);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(1);
  }
  return unaff_r7;
}


// ==== EXTI1_IRQHandler @ 080248fc

/* vector name kept: only clears pending EXTI line 1 (rotary wake). high */

undefined4 EXTI1_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(2);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(2);
  }
  return unaff_r7;
}


// ==== EXTI2_IRQHandler @ 08024910

/* vector name kept: only clears pending EXTI line 2 (rotary wake). high */

undefined4 EXTI2_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(4);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(4);
  }
  return unaff_r7;
}


// ==== EXTI3_IRQHandler @ 08024924

/* vector name kept: only clears pending EXTI line 3 (rotary wake). high */

undefined4 EXTI3_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(8);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(8);
  }
  return unaff_r7;
}


// ==== EXTI4_IRQHandler @ 08024938

/* vector name kept: only clears pending EXTI line 4 (rotary wake). high */

undefined4 EXTI4_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(0x10);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x10);
  }
  return unaff_r7;
}


// ==== EXTI9_5_IRQHandler @ 0802494c

/* vector name kept: only clears pending EXTI lines 5..9 (rotary wake from STOP; no other work).
   high */

undefined4 EXTI9_5_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(0x20);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x20);
  }
  iVar1 = EXTI_GetFlagStatus(0x40);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x40);
  }
  iVar1 = EXTI_GetFlagStatus(0x80);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x80);
  }
  iVar1 = EXTI_GetFlagStatus(0x100);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x100);
  }
  iVar1 = EXTI_GetFlagStatus(0x200);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x200);
  }
  return unaff_r7;
}


// ==== EXTI15_10_IRQHandler @ 080249a8

/* vector name kept: only clears pending EXTI lines 10..15 (rotary wake from STOP; no other work).
   high */

undefined4 EXTI15_10_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = EXTI_GetFlagStatus(0x400);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x400);
  }
  iVar1 = EXTI_GetFlagStatus(0x800);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x800);
  }
  iVar1 = EXTI_GetFlagStatus(0x1000);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x1000);
  }
  iVar1 = EXTI_GetFlagStatus(0x2000);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x2000);
  }
  iVar1 = EXTI_GetFlagStatus(0x4000);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x4000);
  }
  iVar1 = EXTI_GetFlagStatus(0x8000);
  if (iVar1 != 0) {
    EXTI_ClearITPendingBit(0x8000);
  }
  return unaff_r7;
}


// ==== ADC1_IRQHandler @ 08024a24  [ADC_SR]

/* vector name kept: ADC_GetITStatus(0x106) -> ADC_ClearITPendingBit; no work (ADC IRQ never enabled
   by Board_NVICInit). high */

undefined4 ADC1_IRQHandler(void)

{
  int iVar1;
  undefined4 unaff_r7;
  
  iVar1 = ADC_GetITStatus(&ADC_SR,0x106);
  if (iVar1 != 0) {
    ADC_ClearITPendingBit(&ADC_SR,0x106);
  }
  return unaff_r7;
}


// ==== main @ 08024b40

/* VTOR -> 0x08006000 first, then init calls, then the main loop (never returns). high */

void main(void)

{
  int iVar1;
  
  NVIC_SetVectorTable(0x8000000,0x6000);
  Board_Init();
  Board_ProbePF8();
  App_StateInit();
  Board_LCDInit(g_lcd_contrast,1);
  BootScreen();
  do {
    HY_Poll();
    if (g_display_update != '\0') {
      Display_Update();
      if (g_mode == '\0') {
        LowZ_AutoAcDc();
      }
      DAT_20008061 = 1;
    }
    LCD_RefreshDirty();
    if (g_tick10ms != '\0') {
      g_tick10ms = '\0';
      Rotary_Poll();
      Keys_Process();
      Jack_Poll();
      Counter_AcGateAlternate();
      VA_SubAlternate();
      RTC_PollSeconds();
    }
    if (DAT_200080d1 != '\0') {
      DAT_200080d1 = '\0';
      Settings_LoadClock();
      Analog_PollAlternate();
      Record_Snapshot();
    }
    if (DAT_200080d2 != '\0') {
      DAT_200080d2 = '\0';
    }
    if ((DAT_2000802c != '\0') && (DAT_2000802c = '\0', g_ble_on != '\0')) {
      BLE_BuildPacket_Thunk();
    }
    if ((DAT_200080d3 != '\0') && (DAT_200080d3 = '\0', DAT_2000806e != '\0')) {
      Setup_EditTimeout();
    }
    if (DAT_200080d4 != '\0') {
      DAT_200080d4 = '\0';
      Settings_LoadClock();
      DAT_20007ffa = 0;
      if (((DAT_20007fe1 != 0) && (g_log_interval == 0)) && (DAT_20008050 == '\0')) {
        DAT_20007fa8 = 0;
      }
      if (((DAT_20007fe1 != 0) && (g_log_interval != 0)) &&
         ((DAT_20008050 == '\0' && (DAT_20007fa8 = DAT_20007fa8 + 1, g_log_interval <= DAT_20007fa8)
          ))) {
        DAT_20007fa8 = 0;
        DAT_20007fe2 = 1;
      }
      if (((g_apo_off == '\0') && (DAT_2000804e == '\0')) && (DAT_20007fe1 == 0)) {
        g_apo_seconds = g_apo_seconds - 1;
        if (g_apo_seconds == 0) {
          DAT_2000803a = '\x01';
        }
        else if (g_apo_seconds == 0x3c) {
          DAT_200080e8 = '\x01';
        }
      }
      if (DAT_200080e8 != '\0') {
        DAT_200080e8 = '\0';
        DAT_200080e9 = '\x05';
      }
      if (DAT_200080e9 != '\0') {
        Buzzer_OnAlways(500);
        DAT_200080e9 = DAT_200080e9 + -1;
      }
      DAT_200080e6 = g_apo_seconds < 0x3c;
      if ((bool)DAT_200080e6) {
        LCD_BlinkApoIcon();
      }
    }
    if ((DAT_20007fe1 & DAT_20007fe2) != 0) {
      Log_StoreSample();
    }
    if (DAT_20008030 != '\0') {
      BLE_ProcessRx();
    }
    if (DAT_2000801c != '\0') {
      iVar1 = Log_WriteRetry();
      if (iVar1 != 0) {
        DAT_20007fe1 = 0;
        SD_PowerOFF();
        Display_ResetToLive();
      }
      DAT_2000801c = '\0';
    }
    if (((((DAT_2000804a != '\0' || DAT_2000804b != '\0') && (Mode_ApplyChange(1), g_mode != '\x06')
          ) && (g_mode != '\a')) && ((g_mode != '\b' && (g_mode != '\f')))) &&
       (DAT_20008050 != '\0')) {
      g_display_update = '\0';
      DAT_20008060 = 0;
      DAT_20008062 = 0;
      DAT_20008054 = 1;
    }
    if (DAT_2000803a != '\0') {
      App_Sleep();
    }
  } while( true );
}


// ==== App_StateInit @ 08024de0

/* settings + cal from EEPROM, measurement state zeroed (mode, range, MIN/MAX, HOLD, REL, PEAK, LPF,
   AC+DC, VA). high */

undefined4 App_StateInit(void)

{
  undefined4 unaff_r7;
  
  Settings_LoadClock();
  g_variant_ohm6 = Settings_VariantOhm6();
  g_variant_acv0 = Settings_VariantAcv0();
  Cal_LoadRecord(200,0);
  Settings_LoadLogInterval();
  Settings_LoadContrast();
  Settings_LoadApo();
  Settings_LoadTempUnit();
  Settings_LoadDiodeV();
  Settings_LoadContinuity();
  Settings_LoadVaBurden();
  Settings_LoadBuzzer();
  Settings_LoadDateStamp();
  Settings_LoadMeterId();
  Log_RestoreCounters();
  APO_Reset();
  DAT_2000803b = 0;
  g_range = 0;
  g_mode = 0;
  g_cal_mode = 0;
  g_factory_mode = 0;
  Setup_InitialPage();
  DAT_2000804e = 0;
  DAT_2000804f = 0;
  DAT_20008050 = 0;
  DAT_20008058 = 0;
  DAT_200080b7 = 0;
  g_lpf_1khz = 0;
  g_diode_15v = 0;
  DAT_200080b1 = 0;
  DAT_200080b2 = 0;
  DAT_200080ba = 0;
  g_tdm_channel = 0;
  DAT_200080c5 = 0;
  DAT_20008065 = 0;
  g_rotary = 0;
  if (DAT_20008038 != '\0') {
    DAT_20008072 = 0x33;
    DAT_20008073 = 10;
  }
  DAT_20008069 = 0;
  DAT_2000806d = 0;
  LCD_ApplyContrast();
  return unaff_r7;
}


// ==== App_Sleep @ 08024eca

/* auto power-off only (main: g_apo_seconds -> 0): BLE off, LowPower_Prepare, PWR_EnterSTOPMode;
   dial turn wakes it and re-runs Board_Init, App_StateInit, Board_LCDInit, BootScreen. Dial OFF
   cuts the battery in hardware. high */

undefined4 App_Sleep(void)

{
  undefined4 unaff_r7;
  
  App_OffNotice();
  DAT_2000803a = 0;
  g_ble_on = 0;
  LowPower_Prepare();
  PWR_EnterSTOPMode(1,2);
  DAT_20008038 = 1;
  Board_Init();
  App_StateInit();
  Board_LCDInit(g_lcd_contrast,1);
  BootScreen();
  return unaff_r7;
}


// ==== App_OffNotice @ 08024f06  [GPIOE]

/* from App_Sleep before STOP: "-oFF-" on the main display (08017092), PE7 (buzzer) high for 100 x
   10 ms ticks = 1 s, then low; whether a steady PE7 level sounds the buzzer is not confirmed.
   medium */

undefined4 App_OffNotice(void)

{
  undefined4 in_r3;
  byte bVar1;
  
  bVar1 = 0;
  LCD_ShowOff();
  GPIOE.BSRR._0_2_ = 0x80;
  do {
    do {
    } while (g_tick10ms == '\0');
    g_tick10ms = '\0';
    bVar1 = bVar1 + 1;
  } while (bVar1 < 100);
  GPIOE.BSRR._2_2_ = 0x80;
  return in_r3;
}


// ==== SystemInit @ 08025f70  [RCC SCB_VTOR]

/* CMSIS SystemInit: RCC reset, SetSysClock, then VTOR = 0x08000000 (the BOOTLOADER's table; main
   moves it). high */

undefined4 SystemInit(void)

{
  dword dVar1;
  undefined4 unaff_r7;
  
  dVar1 = RCC.CR;
  RCC.CR = dVar1 | 0x100;
  dVar1 = RCC.CFGR;
  RCC.CFGR = dVar1 & 0x88ffc00c;
  dVar1 = RCC.CR;
  RCC.CR = dVar1 & 0xeefefffe;
  dVar1 = RCC.CR;
  RCC.CR = dVar1 & 0xfffbffff;
  dVar1 = RCC.CFGR;
  RCC.CFGR = dVar1 & 0xff02ffff;
  RCC.CIR = 0;
  SetSysClock();
  SCB_VTOR = 0x8000000;
  return unaff_r7;
}


// ==== SetSysClock @ 08025fc2  [Flash PWR RCC]

/* HSE 4 MHz (X2) on, 0x5000-poll timeout; flash 64-bit + prefetch + 1 WS; PWR range 1; PLL HSE x12
   /3 = 16 MHz SYSCLK. high */

void SetSysClock(void)

{
  dword dVar1;
  int local_4;
  
  local_4 = 0;
  dVar1 = RCC.CR;
  RCC.CR = dVar1 | 0x10000;
  do {
    dVar1 = RCC.CR;
    local_4 = local_4 + 1;
    if ((dVar1 & 0x20000) != 0) break;
  } while (local_4 != 0x5000);
  dVar1 = RCC.CR;
  if ((int)(dVar1 << 0xe) < 0) {
    dVar1 = Flash.ACR;
    Flash.ACR = dVar1 | 4;
    dVar1 = Flash.ACR;
    Flash.ACR = dVar1 | 2;
    dVar1 = Flash.ACR;
    Flash.ACR = dVar1 | 1;
    dVar1 = RCC.APB1ENR;
    RCC.APB1ENR = dVar1 | 0x10000000;
    PWR.CR = 0x800;
    do {
      dVar1 = PWR.CSR;
    } while ((int)(dVar1 << 0x1b) < 0);
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1;
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1;
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1;
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1 & 0xff02ffff;
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1 | 0x910000;
    dVar1 = RCC.CR;
    RCC.CR = dVar1 | 0x1000000;
    do {
      dVar1 = RCC.CR;
    } while (-1 < (int)(dVar1 << 6));
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1 & 0xfffffffc;
    dVar1 = RCC.CFGR;
    RCC.CFGR = dVar1 | 3;
    do {
      dVar1 = RCC.CFGR;
    } while ((dVar1 & 0xc) != 0xc);
  }
  return;
}


// ==== __iar_zero_init3 @ 08028cdc

/* IAR runtime: init-table entry (table 0x08029078 -> 0x08028cdd, size 0x6EBC, dest 0x20001240):
   branches to the loop test; zeroes {size, addr} pairs, odd addr = r9-relative. library */

void __iar_zero_init3(uint *param_1)

{
  uint *puVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  uint uVar4;
  uint uVar5;
  int unaff_r9;
  
  while( true ) {
    puVar1 = param_1 + 1;
    uVar5 = *param_1;
    if (uVar5 == 0) break;
    param_1 = param_1 + 2;
    puVar2 = (undefined4 *)*puVar1;
    if ((int)puVar2 << 0x1f < 0) {
      puVar2 = (undefined4 *)(unaff_r9 + -1 + (int)puVar2);
    }
    do {
      uVar4 = uVar5;
      puVar3 = puVar2;
      puVar2 = puVar3 + 1;
      *puVar3 = 0;
      uVar5 = uVar4 - 4;
    } while (3 < uVar4 - 4);
    if ((int)(uVar4 << 0x1e) < 0) {
      *(undefined2 *)puVar2 = 0;
      puVar2 = (undefined4 *)((int)puVar3 + 6);
    }
    if ((int)(uVar4 << 0x1f) < 0) {
      *(undefined1 *)puVar2 = 0;
    }
  }
  return;
}


// ==== __iar_zero_init3_next @ 08028d12

/* loop head inside __iar_zero_init3 (Ghidra split): load next size, 0 ends, else back into the zero
   loop; not a separate function. library */

void __iar_zero_init3_next(uint *param_1)

{
  uint *puVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  uint uVar4;
  uint uVar5;
  int unaff_r9;
  
  while( true ) {
    puVar1 = param_1 + 1;
    uVar5 = *param_1;
    if (uVar5 == 0) break;
    param_1 = param_1 + 2;
    puVar2 = (undefined4 *)*puVar1;
    if ((int)puVar2 << 0x1f < 0) {
      puVar2 = (undefined4 *)(unaff_r9 + -1 + (int)puVar2);
    }
    do {
      uVar4 = uVar5;
      puVar3 = puVar2;
      puVar2 = puVar3 + 1;
      *puVar3 = 0;
      uVar5 = uVar4 - 4;
    } while (3 < uVar4 - 4);
    if ((int)(uVar4 << 0x1e) < 0) {
      *(undefined2 *)puVar2 = 0;
      puVar2 = (undefined4 *)((int)puVar3 + 6);
    }
    if ((int)(uVar4 << 0x1f) < 0) {
      *(undefined1 *)puVar2 = 0;
    }
  }
  return;
}


// ==== __iar_data_init3 @ 08028fe4

/* IAR init-table walker: zero 0x6EBC B at 0x20001240, LZ-unpack 0x348 B to 0x20000000. [video] */

void __iar_data_init3(void)

{
  int *piVar1;
  
  for (piVar1 = &DAT_08029078; piVar1 != &DAT_08029098;
      piVar1 = (int *)(*(code *)((int)piVar1 + *piVar1))(piVar1 + 1)) {
  }
  return;
}


// ==== __iar_program_start @ 0802910c

/* IAR C start: data init, then main; never returns (Reset_Handler decompiled garbage past it
   before). high [video] */

void __iar_program_start(void)

{
  int iVar1;
  
  iVar1 = __low_level_init();
  if (iVar1 != 0) {
    __iar_data_init3();
  }
                    /* WARNING: Subroutine does not return */
  main(0);
}


// ==== __low_level_init @ 08029126

/* IAR runtime default: returns 1 (do data init); called first by __iar_program_start. library */

undefined4 __low_level_init(void)

{
  return 1;
}


// ==== Reset_Handler @ 08029174

/* vector name kept: SystemInit(), __iar_program_start() (never returns). high */

void Reset_Handler(void)

{
  SystemInit();
                    /* WARNING: Subroutine does not return */
  __iar_program_start();
}


// ==== WWDG_IRQHandler @ 08029298

/* IAR startup PUBWEAK default: b.w self (hang). library */

void WWDG_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== PVD_IRQHandler @ 0802929c

/* IAR startup PUBWEAK default: b.w self. library */

void PVD_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TAMPER_STAMP_IRQHandler @ 080292a0

/* IAR startup PUBWEAK default: b.w self. library */

void TAMPER_STAMP_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== FLASH_IRQHandler @ 080292a4

/* IAR startup PUBWEAK default: b.w self. library */

void FLASH_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== RCC_IRQHandler @ 080292a8

/* IAR startup PUBWEAK default: b.w self. library */

void RCC_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel1_IRQHandler @ 080292ac

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel1_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel2_IRQHandler @ 080292b0

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel2_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel3_IRQHandler @ 080292b4

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel3_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel4_IRQHandler @ 080292b8

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel4_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel5_IRQHandler @ 080292bc

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel5_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel6_IRQHandler @ 080292c0

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel6_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA1_Channel7_IRQHandler @ 080292c4

/* IAR startup PUBWEAK default: b.w self. library */

void DMA1_Channel7_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== USB_HP_IRQHandler @ 080292c8

/* IAR startup PUBWEAK default: b.w self. library */

void USB_HP_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== USB_LP_IRQHandler @ 080292cc

/* IAR startup PUBWEAK default: b.w self. library */

void USB_LP_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DAC_IRQHandler @ 080292d0

/* IAR startup PUBWEAK default: b.w self. library */

void DAC_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== COMP_CA_IRQHandler @ 080292d4

/* IAR startup PUBWEAK default: b.w self. library */

void COMP_CA_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== LCD_IRQHandler @ 080292d8

/* IAR startup PUBWEAK default: b.w self. library */

void LCD_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM9_IRQHandler @ 080292dc

/* IAR startup PUBWEAK default: b.w self. library */

void TIM9_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM10_IRQHandler @ 080292e0

/* IAR startup PUBWEAK default: b.w self. library */

void TIM10_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM11_IRQHandler @ 080292e4

/* IAR startup PUBWEAK default: b.w self. library */

void TIM11_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM4_IRQHandler @ 080292e8

/* IAR startup PUBWEAK default: b.w self. library */

void TIM4_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== I2C1_EV_IRQHandler @ 080292ec

/* IAR startup PUBWEAK default: b.w self. library */

void I2C1_EV_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== I2C1_ER_IRQHandler @ 080292f0

/* IAR startup PUBWEAK default: b.w self. library */

void I2C1_ER_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== I2C2_EV_IRQHandler @ 080292f4

/* IAR startup PUBWEAK default: b.w self. library */

void I2C2_EV_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== I2C2_ER_IRQHandler @ 080292f8

/* IAR startup PUBWEAK default: b.w self. library */

void I2C2_ER_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== SPI1_IRQHandler @ 080292fc

/* IAR startup PUBWEAK default: b.w self. library */

void SPI1_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== SPI2_IRQHandler @ 08029300

/* IAR startup PUBWEAK default: b.w self. library */

void SPI2_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== USART1_IRQHandler @ 08029304

/* IAR startup PUBWEAK default: b.w self. library */

void USART1_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== USART3_IRQHandler @ 08029308

/* IAR startup PUBWEAK default: b.w self. library */

void USART3_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== RTC_Alarm_IRQHandler @ 0802930c

/* IAR startup PUBWEAK default: b.w self. library */

void RTC_Alarm_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== USB_FS_WKUP_IRQHandler @ 08029310

/* IAR startup PUBWEAK default: b.w self. library */

void USB_FS_WKUP_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM6_IRQHandler @ 08029314

/* IAR startup PUBWEAK default: b.w self. library */

void TIM6_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM7_IRQHandler @ 08029318

/* IAR startup PUBWEAK default: b.w self. library */

void TIM7_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== TIM5_IRQHandler @ 0802931c

/* IAR startup PUBWEAK default: b.w self. library */

void TIM5_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== SPI3_IRQHandler @ 08029320

/* IAR startup PUBWEAK default: b.w self. library */

void SPI3_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== UART4_IRQHandler @ 08029324

/* vector 64 = IRQ 48 (UART4 on STM32L1 high density; was Vector64_Handler); PUBWEAK default b.w
   self. library */

void UART4_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== UART5_IRQHandler @ 08029328

/* vector 65 = IRQ 49 (UART5; was Vector65_Handler); PUBWEAK default b.w self. library */

void UART5_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA2_CH1_IRQHandler @ 0802932c

/* IAR startup PUBWEAK default: b.w self. library */

void DMA2_CH1_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA2_CH2_IRQHandler @ 08029330

/* IAR startup PUBWEAK default: b.w self. library */

void DMA2_CH2_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA2_CH3_IRQHandler @ 08029334

/* IAR startup PUBWEAK default: b.w self. library */

void DMA2_CH3_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== DMA2_CH5_IRQHandler @ 08029338

/* IAR startup PUBWEAK default: b.w self. library */

void DMA2_CH5_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


// ==== AES_IRQHandler @ 0802933c

/* IAR startup PUBWEAK default: b.w self (vector 71 = IRQ 55). library */

void AES_IRQHandler(void)

{
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}


