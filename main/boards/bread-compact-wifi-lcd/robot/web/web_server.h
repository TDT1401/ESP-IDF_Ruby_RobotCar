#pragma once

// Called from this board's serialized Wi-Fi event callback, after motor initialization.
// The server and its handlers live until reboot. Disconnect immediately revokes motion.
void UpdateL298nWebNetwork(bool connected);
