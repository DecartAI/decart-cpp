// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Auth example: mint a short-lived client token from a server-side API key.
//
//   DECART_API_KEY=sk-... ./create_token
//
#include <decart/decart.h>

#include <cstdlib>
#include <iostream>

int main() {
  if (std::getenv("DECART_API_KEY") == nullptr) {
    std::cerr << "Set DECART_API_KEY to run this example.\n";
    return 1;
  }

  try {
    decart::Client client;

    decart::CreateTokenOptions options;
    options.expiresIn = 300; // 5 minutes
    options.allowedModels = {"lucy-restyle-2"};
    options.metadata = {{"role", "viewer"}};

    const decart::CreateTokenResponse token = client.tokens().create(options);
    std::cout << "apiKey:    " << token.apiKey << "\n";
    std::cout << "expiresAt: " << token.expiresAt << "\n";
    return 0;
  } catch (const decart::Exception& e) {
    std::cerr << "Decart error [" << decart::toString(e.code()) << "]: " << e.what() << "\n";
    return 1;
  }
}
