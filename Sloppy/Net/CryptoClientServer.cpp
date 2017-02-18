/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <thread>

#include "../libSloppy.h"
#include "CryptoClientServer.h"

namespace Sloppy
{
  namespace Net
  {
    const string CryptoClientServer::MagicPhrase{"LetMeInPlease"};

    void CryptoClientServer::CryptoServer::doTheWork()
    {
      bool authSuccess = doAuthProcess();
      if (!authSuccess)
      {
        cerr << "ServerWorker: authentication failed!!" << endl;
        closeSocket();
        return;   // unconditionally quit if anything goes wrong
      }

      this_thread::sleep_for(chrono::seconds{2});
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoServer::doAuthProcess()
    {
      // Step 1: wait for the client to open with "LetMeInPlease"
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead(MagicPhrase.size(), AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "ServerWorker: AU1, phrase complete" << endl;
      if (data != "LetMeInPlease") return false;
      cout << "ServerWorker: AU1, phrase coorrect" << endl;
      bool isOkay = write("OK");
      if (!isOkay) return false;
      cout << "ServerWorker: AU1, reponse sent" << endl;
      cout << "ServerWorker: AuthStep 1 okay" << endl;

      // Step 2a: wait for public key and challenge
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      MessageDissector d{data};
      ManagedBuffer challengeFromClient;
      string _clientPubKey;
      try
      {
        _clientPubKey = d.getString();
        challengeFromClient = d.getManagedBuffer();
      }
      catch (...)
      {
        return false;
      }
      if (_clientPubKey.size() != crypto_box_PUBLICKEYBYTES) return false;
      SodiumLib::AsymCrypto_PublicKey clientPubKey;
      clientPubKey.fillFromString(_clientPubKey);
      if (challengeFromClient.getSize() != ChallengeSize) return false;

      // Step 2b: send our public key, sign/encrypt the client's challenge,
      // append the used nonce and send our own challenge
      MessageBuilder b;
      b.addManagedMemory(pk);

      SodiumLib::AsymCrypto_Nonce asymNonce;
      sodium->randombytes_buf(asymNonce);
      ManagedBuffer cipher = sodium->crypto_box_easy(challengeFromClient, asymNonce, clientPubKey, sk);
      b.addManagedMemory(cipher);

      b.addManagedMemory(asymNonce);

      ManagedBuffer challengeForClient{ChallengeSize};
      sodium->randombytes_buf(challengeForClient);
      b.addManagedMemory(challengeForClient);

      isOkay = write_framed((const string&)b.getDataAsRef());
      if (!isOkay) return false;

      cout << "ServerWorker: AuthStep 2 okay" << endl;

      return true;
    }

    //----------------------------------------------------------------------------

    void CryptoClientServer::CryptoClient::doTheWork()
    {
      bool authSuccess = doAuthProcess();
      if (!authSuccess)
      {
        cout << "\t\t\tClient: authentication failed!!" << endl;
        closeSocket();
        return;   // unconditionally quit if anything goes wrong
      }

      this_thread::sleep_for(chrono::seconds{2});
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::doAuthProcess()
    {
      // Step 1: present the magic words and wait for an "OK"
      bool isOk = write(MagicPhrase);
      if (!isOk) return false;
      cout << "\t\t\tClient: phrase sent" << endl;
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead(2, AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "\t\t\tClient: Server response complete" << endl;
      if (data != "OK") return false;
      cout << "\t\t\tClient: Server response correct" << endl;
      cout << "\t\t\tClient: AuthStep 1 okay" << endl;

      // Step 2a: transmit our public key and a challenge for the server
      MessageBuilder b;
      b.addString(pk.copyToString());
      ManagedBuffer challenge{ChallengeSize};
      sodium->randombytes_buf(challenge);
      b.addManagedMemory(challenge);
      isOk = write_framed((const string&)b.getDataAsRef());
      if (!isOk) return false;

      // step 2b: expect the server's public key, the signed/encrypted challenge,
      // the used nonce and a challenge for us
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "\t\t\tClient: AuthStep 2, received server response" << endl;

      ManagedBuffer _srvPubKey;
      ManagedBuffer cipherChallenge;
      ManagedBuffer _asymNonce;
      ManagedBuffer challengeFromServer;
      try
      {
        cout << "00" << endl;
        MessageDissector d{data};
        cout << "0" << endl;
        _srvPubKey = d.getManagedBuffer();
        cout << "1" << endl;
        cipherChallenge = d.getManagedBuffer();
        cout << "2" << endl;
        _asymNonce = d.getManagedBuffer();
        cout << "3" << endl;
        challengeFromServer = d.getManagedBuffer();
        cout << "4" << endl;
      }
      catch (...)
      {
        return false;
      }
      cout << "\t\t\tClient: AuthStep 2, dissected server response" << endl;

      if (_srvPubKey.getSize() != crypto_box_PUBLICKEYBYTES) return false;
      cout << "\t\t\tClient: Server public key has correct length" << endl;

      if (cipherChallenge.getSize() != (ChallengeSize + crypto_box_MACBYTES)) return false;
      cout << "\t\t\tClient: Signed and encrypted challenge has correct length" << endl;

      if (_asymNonce.getSize() != crypto_box_NONCEBYTES) return false;
      cout << "\t\t\tClient: Nonce has correct length" << endl;

      if (challengeFromServer.getSize() != ChallengeSize) return false;
      cout << "\t\t\tClient: Challenge from server has correct length" << endl;

      SodiumLib::AsymCrypto_PublicKey srvPubKey;
      srvPubKey.fillFromManagedMemory(_srvPubKey);

      SodiumLib::AsymCrypto_Nonce asymNonce;
      asymNonce.fillFromManagedMemory(_asymNonce);

      // check the encrypted challenge
      // THIS PROVES THAT THE SERVER ACTUALLY CONTROLS THE PRIVATE KEY
      auto returnedChallenge = sodium->crypto_box_open_easy(cipherChallenge, asymNonce, srvPubKey, sk);
      if (!(returnedChallenge.isValid())) return false;
      cout << "\t\t\tClient: returned challenge from server has valid signature" << endl;
      if (!(sodium->memcmp(challenge, returnedChallenge))) return false;
      cout << "\t\t\tClient: returned challenge from server has correct content" << endl;

      cout << "\t\t\tClient: AuthStep 2 okay" << endl;

      return true;
    }

  }
}
