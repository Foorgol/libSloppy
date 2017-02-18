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

    bool CryptoClientServer::CryptoServer::authStep1()
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
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoServer::authStep2()
    {
      // Step 2a: wait for public key and challenge
      PreemptiveReadResult rr;
      string data;
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
      clientPubKey.fillFromString(_clientPubKey);
      if (challengeFromClient.getSize() != ChallengeSize) return false;

      // Step 2b: send our public key, sign/encrypt the client's challenge,
      // append the used nonce and send our own challenge
      MessageBuilder b;
      b.addManagedMemory(pk);

      sodium->randombytes_buf(asymNonce);
      ManagedBuffer cipher = sodium->crypto_box_easy(challengeFromClient, asymNonce, clientPubKey, sk);
      b.addManagedMemory(cipher);

      b.addManagedMemory(asymNonce);

      sodium->randombytes_buf(challengeForClient);
      b.addManagedMemory(challengeForClient);

      bool isOkay = write_framed((const string&)b.getDataAsRef());
      if (!isOkay) return false;

      cout << "ServerWorker: AuthStep 2 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoServer::authStep3()
    {
      // Step 3a: wait for encrypted/signed challenge, symmetric nonce and
      // public DH parameter from the client
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "ServerWorker: received Auth3 data from client" << endl;

      MessageDissector d{data};
      ManagedBuffer cipherChallenge;
      ManagedBuffer signedPubDHKey;
      try
      {
        cipherChallenge = d.getManagedBuffer();
        if (!(symNonce.fillFromManagedMemory(d.getManagedBuffer()))) return false;
        signedPubDHKey = d.getManagedBuffer();
      }
      catch (...)
      {
        return false;
      }
      cout << "ServerWorker: dissected Auth3 data from client" << endl;

      if (cipherChallenge.getSize() != (ChallengeSize + crypto_box_MACBYTES)) return false;
      cout << "ServerWorker: encrypted cipher has correct size" << endl;

      if (signedPubDHKey.getSize() != (crypto_scalarmult_BYTES + crypto_box_MACBYTES)) return false;
      cout << "ServerWorker: encrypted DH key has correct size" << endl;

      sodium->increment(asymNonce);
      SodiumSecureMemory returnedChallenge = sodium->crypto_box_open_easy(cipherChallenge, asymNonce, clientPubKey, sk);
      if (!(returnedChallenge.isValid())) return false;
      cout << "ServerWorker: encrypted cipher has valid signature" << endl;

      if (!(sodium->memcmp(returnedChallenge, challengeForClient))) return false;
      cout << "ServerWorker: returned cipher from client matches source" << endl;

      // the client has proven that it actually controls the private key. check if the
      // client to be accepted
      if (!(isClientAcceptable(clientPubKey))) return false;
      cout << "ServerWorker: client's public key accepted" << endl;

      // derive the session key from the DH parameter
      sodium->increment(asymNonce);
      SodiumSecureMemory pubDh = sodium->crypto_box_open_easy(signedPubDHKey, asymNonce, clientPubKey, sk);
      if (!(pubDh.isValid())) return false;
      cout << "ServerWorker: client's public DH key has valid signature" << endl;

      SodiumLib::DH_PublicKey pubDHKey;
      if (!(pubDHKey.fillFromManagedMemory(pubDh))) return false;
      cout << "ServerWorker: client's public DH key has valid size" << endl;

      sessionKey = dhEx.getSharedSecret(pubDHKey);
      cout << "ServerWorker: session key is " << toBase64(sessionKey) << endl;
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);

      //
      // step 3b: send our signed public DH key
      //
      sodium->increment(asymNonce);
      ManagedBuffer signedDH = sodium->crypto_box_easy(dhEx.getMyPublicKey(), asymNonce, clientPubKey, sk);
      bool isOk = write_framed(signedDH.copyToString());
      if (!isOk) return false;
      cout << "ServerWorker: sent signed DH key" << endl;

      cout << "ServerWorker: AuthStep 3 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoServer::doAuthProcess()
    {
      if (!(authStep1())) return false;
      if (!(authStep2())) return false;
      if (!(authStep3())) return false;

      cout << "ServerWorker: !!! Authentication finished, switching to symmetric encryption !!! " << endl;

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

    void CryptoClientServer::CryptoClient::setExpectedServerKey(const CryptoClientServer::PubKey& srvPubKey)
    {
      expectedServerKey = PubKey::asCopy(srvPubKey);
      hasExpectedServerKey = true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::doAuthProcess()
    {
      if (!(authStep1())) return false;
      if (!(authStep2())) return false;
      if (!(authStep3())) return false;

      cout << "\t\t\tClient: !!! Authentication finished, switching to symmetric encryption !!! " << endl;

      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::authStep1()
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
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::authStep2()
    {
      // Step 2a: transmit our public key and a challenge for the server
      MessageBuilder b;
      b.addString(pk.copyToString());
      ManagedBuffer challenge{ChallengeSize};
      sodium->randombytes_buf(challenge);
      b.addManagedMemory(challenge);
      bool isOk = write_framed((const string&)b.getDataAsRef());
      if (!isOk) return false;

      // step 2b: expect the server's public key, the signed/encrypted challenge,
      // the used nonce and a challenge for us
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "\t\t\tClient: AuthStep 2, received server response" << endl;

      ManagedBuffer cipherChallenge;
      try
      {
        MessageDissector d{data};

        if (!(srvPubKey.fillFromManagedMemory(d.getManagedBuffer()))) return false;;
        cipherChallenge = d.getManagedBuffer();
        if (!(asymNonce.fillFromManagedMemory(d.getManagedBuffer()))) return false;;
        challengeFromServer = d.getManagedBuffer();
      }
      catch (...)
      {
        return false;
      }
      cout << "\t\t\tClient: AuthStep 2, dissected server response" << endl;

      if (cipherChallenge.getSize() != (ChallengeSize + crypto_box_MACBYTES)) return false;
      cout << "\t\t\tClient: Signed and encrypted challenge has correct length" << endl;

      if (challengeFromServer.getSize() != ChallengeSize) return false;
      cout << "\t\t\tClient: Challenge from server has correct length" << endl;
      cout << "\t\t\tClient: Challenge from server is " << toBase64(challengeFromServer) << endl;

      // check the encrypted challenge
      // THIS PROVES THAT THE SERVER ACTUALLY CONTROLS THE PRIVATE KEY
      auto returnedChallenge = sodium->crypto_box_open_easy(cipherChallenge, asymNonce, srvPubKey, sk);
      if (!(returnedChallenge.isValid())) return false;
      cout << "\t\t\tClient: returned challenge from server has valid signature" << endl;

      if (!(sodium->memcmp(challenge, returnedChallenge))) return false;
      cout << "\t\t\tClient: returned challenge from server has correct content" << endl;

      // call the hook for authorizing the server's public key
      if (!(isServerAcceptable(srvPubKey))) return false;
      cout << "\t\t\tClient: server's public key (==> identity!) accepted!" << endl;

      cout << "\t\t\tClient: AuthStep 2 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::authStep3()
    {
      // step 3a: send the encrypted and signed challenge from the server
      // back to the server; append an initial nonce for later symmetric
      // encryption and public DH parameter (signed by the client)
      sodium->increment(asymNonce);
      ManagedBuffer cipherChallenge = sodium->crypto_box_easy(challengeFromServer, asymNonce, srvPubKey, sk);

      sodium->randombytes_buf(symNonce);

      MessageBuilder b;
      b.addManagedMemory(cipherChallenge);
      b.addManagedMemory(symNonce);

      sodium->increment(asymNonce);
      auto signedAndEncryptedDH = sodium->crypto_box_easy(dhEx.getMyPublicKey(), asymNonce, srvPubKey, sk);
      b.addManagedMemory(signedAndEncryptedDH);

      bool isOk = write_framed((const string&)b.getDataAsRef());
      if (!isOk) return false;
      cout << "\t\t\tClient: AuthStep 3 data sent" << endl;

      //
      // step 3b: wait for the server's signed public DH key
      //
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "\t\t\tClient: AuthStep 3, received server response" << endl;

      if (data.size() != (crypto_scalarmult_BYTES + crypto_box_MACBYTES)) return false;
      cout << "\t\t\tClient: server's signed DH key has valid length" << endl;

      sodium->increment(asymNonce);
      string _pubDh = sodium->crypto_box_open_easy(data, asymNonce, srvPubKey, sk);
      if (_pubDh.empty()) return false;
      cout << "\t\t\tClient: server's signed DH key has valid signature" << endl;

      SodiumLib::DH_PublicKey pubDH;
      if (!(pubDH.fillFromString(_pubDh))) return false;
      sessionKey = dhEx.getSharedSecret(pubDH);
      cout << "\t\t\tClient: session key is " << toBase64(sessionKey) << endl;
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);

      cout << "\t\t\tClient: AuthStep 3 okay" << endl;
      return true;

    }

    //----------------------------------------------------------------------------

    bool CryptoClientServer::CryptoClient::cmpServerKeys(const PubKey& srvPubKey) const
    {
      if (!hasExpectedServerKey) return false;

      bool isEq = (srvPubKey == expectedServerKey);
      if (!isEq)
      {
        string s = "Client: public server key mismatch!\n";
        s += "Expected key:  " + toBase64(expectedServerKey) + "\n";
        s += "Presented key: " + toBase64(srvPubKey) + "\n";
        cerr << s;
      }
      return isEq;
    }

  }
}
