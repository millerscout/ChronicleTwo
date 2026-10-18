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

// Minimum abs.now for refunds at the current level (weapon level does not decrease).
float AbsBankWeaponRefundFloor(const CGameDataUsed *weapon);

// Moves up to amount ABS from bank to weapon; returns amount transferred. May level up.
float AbsBankTransferToWeapon(CGameDataUsed *weapon, float amount);

// Moves up to amount ABS from weapon to bank; returns amount refunded. Never lowers weapon level.
float AbsBankTransferFromWeapon(CGameDataUsed *weapon, float amount);

// When banking is on, credits the orb and returns true (caller must skip AddExpWeaponParam).
bool AbsBankRouteOrbExperience(float amount);
