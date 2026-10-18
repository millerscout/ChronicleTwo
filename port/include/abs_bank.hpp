#pragma once

class CGameDataUsed;

bool AbsBankEnabled();

float AbsBankBalance();

void AbsBankCredit(float amount);

bool AbsBankDebit(float amount);

// Pending ABS to apply on the weapon stats panel (clamped when applied).
float AbsBankApplyAmount();

void AbsBankSetApplyAmount(float amount);

void AbsBankResetApplyAmount();

// Applies pending amount to weapon; debits bank by the amount actually added. Returns false if nothing applied.
bool AbsBankApplyToWeapon(CGameDataUsed *weapon);

// When banking is on, credits the orb and returns true (caller must skip AddExpWeaponParam).
bool AbsBankRouteOrbExperience(float amount);
