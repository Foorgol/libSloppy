/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
    //*********************************************************************
    //
    // Common methods for client and serer
    //
    //*********************************************************************
    const string CryptoClientServer::MagicPhrase{"LetMeInPlease"};

    bool CryptoClientServer::encryptAndWrite(const ManagedMemory& msg)
    {
      sodium->increment(symNonce);
      sessionKey.setAccess(SodiumSecureMemAccess::RO);
      ManagedBuffer cipher = sodium->crypto_secretbox_easy(msg, symNonce, sessionKey);
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);
      if (!(cipher.isValid()))
      {
        cerr << "CryptoClientServer: error when encrypting response to peer! No data sent!" << endl;
        return false;
      }
      return write_framed_MB(cipher);
    }

    //----------------------------------------------------------------------------

    pair<PreemptiveReadResult, ManagedBuffer> CryptoClientServer::readAndDecrypt(size_t timeout_ms)
    {
      PreemptiveReadResult rr;
      ManagedBuffer cipher;
      tie(rr, cipher) = preemptiveRead_framed_MB(timeout_ms);

      if (rr != PreemptiveReadResult::Complete)
      {
        cerr << "CryptoClientServer: error while waiting for data (interrupted, incomplete, timeout or error)" << endl;
        return make_pair(rr, ManagedBuffer{});
      }

      // decrypt the data
      sodium->increment(symNonce);
      sessionKey.setAccess(SodiumSecureMemAccess::RO);
      auto plain = sodium->crypto_secretbox_open_easy(cipher, symNonce, sessionKey);
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);

      // quit if the MAC was invalid
      if (!(plain.isValid()))
      {
        cerr << "CryptoClientServer: received invalid / corrupted data from peer ==> error!" << endl;
        return make_pair(PreemptiveReadResult::Error, ManagedBuffer{});
      }

      return make_pair(rr, std::move(plain));
    }





    //*********************************************************************
    //
    // The __SERVER__
    //
    //*********************************************************************

    pair<CryptoClientServer::ResponseReaction, ManagedBuffer> CryptoServer::handleRequest(const ManagedBuffer& reqData)
    {
      // dummy response, this should be overriden by derived classes
      return make_pair(ResponseReaction::QuitWithoutSending, ManagedBuffer{});
    }

    //----------------------------------------------------------------------------

    void CryptoServer::doTheWork()
    {
      bool authSuccess = doAuthProcess();
      if (!authSuccess)
      {
        cerr << "ServerWorker: authentication failed!!" << endl;
        closeSocket();
        return;   // unconditionally quit if anything goes wrong
      }

      cout << "ServerWorker: entering main request-response-loop" << endl;
      while (true)
      {
        // wait (infinitely) for the next request and
        // terminate upon external request
        ManagedBuffer req;
        PreemptiveReadResult rr;
        tie(rr, req) = readAndDecrypt(0);

        if (rr == PreemptiveReadResult::Interrupted)
        {
          cerr << "ServerWorker: received termination request!" << endl;
          break;
        }
        if (rr != PreemptiveReadResult::Complete)
        {
          cerr << "ServerWorker: error while waiting for data ==> terminating!" << endl;
          break;
        }

        cout << "ServerWorker: received request, " << req.getSize() << " bytes" << endl;

        // forward the valid request to the request handler
        ResponseReaction respCode;
        ManagedBuffer data;
        try
        {
          //tie(respCode, data) = handleRequest(req);
          tie(respCode, data) = handleRequest(req);
          cout << "ServerWorker: handler returned " << data.getSize() << " bytes of unencrypted data" << endl;
        }
        catch (...)
        {
          cerr << "ServerWorker: request handler caused unexpected exception. Terminating!" << endl;
          break;
        }

        // decide what to do based on the response code
        if (respCode == ResponseReaction::ContinueWithoutSending) continue;
        if (respCode == ResponseReaction::QuitWithoutSending) break;

        // send the response
        if (!(encryptAndWrite(data)))
        {
          cerr << "ServerWorker: error when sending response to client ==> terminating!" << endl;
          break;
        }

        cout << "ServerWorker: sent response, " << data.getSize() << " unencrypted bytes" << endl;

        if (respCode == ResponseReaction::SendAndQuit) break;
      }
      cout << "ServerWorker: left main request-response-loop" << endl;
      closeSocket();
    }

    //----------------------------------------------------------------------------

    bool CryptoServer::authStep1()
    {
      // Step 1: wait for the client to open with "LetMeInPlease"
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead(MagicPhrase.size(), AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "ServerWorker: AU1, phrase complete" << endl;
      if (data != MagicPhrase) return false;
      cout << "ServerWorker: AU1, phrase coorrect" << endl;

      // the server responds with the protocol version
      // followed by "OK"
      MessageBuilder m;
      m.addByte(ProtoVersionMajor);
      m.addByte(ProtoVersionMinor);
      m.addByte(ProtoVersionPatch);
      m.addString("OK");
      bool isOkay = write(m);
      if (!isOkay) return false;
      cout << "ServerWorker: AU1, reponse sent" << endl;

      cout << "ServerWorker: AuthStep 1 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoServer::authStep2()
    {
      // Step 2a: wait for public key and challenge
      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead_framed(AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      MessageDissector d{data};
      try
      {
        if (!(peerPubKey.fillFromString(d.getString()))) return false;;
        challengeFromPeer = d.getManagedBuffer();
      }
      catch (...)
      {
        return false;
      }
      if (challengeFromPeer.getSize() != ChallengeSize) return false;

      // Step 2b: send our public key, sign/encrypt the client's challenge,
      // append the used nonce and send our own challenge
      MessageBuilder b;
      b.addManagedMemory(pk);

      sodium->randombytes_buf(asymNonce);
      sk.setAccess(SodiumSecureMemAccess::RO);
      ManagedBuffer cipher = sodium->crypto_box_easy(challengeFromPeer, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      b.addManagedMemory(cipher);

      b.addManagedMemory(asymNonce);

      sodium->randombytes_buf(challengeForPeer);
      b.addManagedMemory(challengeForPeer);

      bool isOkay = write_framed((const string&)b.getDataAsRef());
      if (!isOkay) return false;

      cout << "ServerWorker: AuthStep 2 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoServer::authStep3()
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
      sk.setAccess(SodiumSecureMemAccess::RO);
      SodiumSecureMemory returnedChallenge = sodium->crypto_box_open_easy(cipherChallenge, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      if (!(returnedChallenge.isValid())) return false;
      cout << "ServerWorker: encrypted cipher has valid signature" << endl;

      if (!(sodium->memcmp(returnedChallenge, challengeForPeer))) return false;
      cout << "ServerWorker: returned cipher from client matches source" << endl;

      // the client has proven that it actually controls the private key. check if the
      // client to be accepted
      if (!(isPeerAcceptable(peerPubKey))) return false;
      cout << "ServerWorker: client's public key accepted" << endl;

      // derive the session key from the DH parameter
      sodium->increment(asymNonce);
      sk.setAccess(SodiumSecureMemAccess::RO);
      SodiumSecureMemory pubDh = sodium->crypto_box_open_easy(signedPubDHKey, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      if (!(pubDh.isValid())) return false;
      cout << "ServerWorker: client's public DH key has valid signature" << endl;

      SodiumLib::DH_PublicKey pubDHKey;
      if (!(pubDHKey.fillFromManagedMemory(pubDh))) return false;
      cout << "ServerWorker: client's public DH key has valid size" << endl;

      sessionKey = dhEx.getSharedSecret(pubDHKey);
      //cout << "ServerWorker: session key is " << toBase64(sessionKey) << endl;
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);

      //
      // step 3b: send our signed public DH key
      //
      sodium->increment(asymNonce);
      sk.setAccess(SodiumSecureMemAccess::RO);
      ManagedBuffer signedDH = sodium->crypto_box_easy(dhEx.getMyPublicKey(), asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      bool isOk = write_framed(signedDH.copyToString());
      if (!isOk) return false;
      cout << "ServerWorker: sent signed DH key" << endl;

      cout << "ServerWorker: AuthStep 3 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoServer::doAuthProcess()
    {
      if (!(authStep1())) return false;
      if (!(authStep2())) return false;
      if (!(authStep3())) return false;

      cout << "ServerWorker: !!! Authentication finished, switching to symmetric encryption !!! " << endl;
      handshakeComplete = true;

      return true;
    }






    //*********************************************************************
    //
    // The __CLIENT__
    //
    //*********************************************************************

    void CryptoClient::setExpectedServerKey(const CryptoClientServer::PubKey& srvPubKey)
    {
      expectedServerKey = PubKey::asCopy(srvPubKey);
      hasExpectedServerKey = true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClient::doAuthProcess()
    {
      if (!(authStep1())) return false;
      if (!(authStep2())) return false;
      if (!(authStep3())) return false;

      cout << "\t\t\tClient: !!! Authentication finished, switching to symmetric encryption !!! " << endl;
      handshakeComplete = true;

      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClient::authStep1()
    {
      // Step 1: present the magic words and wait for an "OK"
      bool isOk = write(MagicPhrase);
      if (!isOk) return false;
      cout << "\t\t\tClient: phrase sent" << endl;

      PreemptiveReadResult rr;
      string data;
      tie(rr, data) = preemptiveRead(3 + 8 + 2, AuthStepTimeout_ms);
      if (rr != PreemptiveReadResult::Complete) return false;
      cout << "\t\t\tClient: Server response complete" << endl;

      MessageDissector md{data};
      if (md.getByte() != ProtoVersionMajor) return false;
      if (md.getByte() != ProtoVersionMinor) return false;
      if (md.getByte() != ProtoVersionPatch) return false;
      if (md.getString() != "OK") return false;
      cout << "\t\t\tClient: Server response correct" << endl;

      cout << "\t\t\tClient: AuthStep 1 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClient::authStep2()
    {
      // Step 2a: transmit our public key and a challenge for the server
      MessageBuilder b;
      b.addManagedMemory(pk);
      sodium->randombytes_buf(challengeForPeer);
      b.addManagedMemory(challengeForPeer);
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

        if (!(peerPubKey.fillFromManagedMemory(d.getManagedBuffer()))) return false;;
        cipherChallenge = d.getManagedBuffer();
        if (!(asymNonce.fillFromManagedMemory(d.getManagedBuffer()))) return false;;
        challengeFromPeer = d.getManagedBuffer();
      }
      catch (...)
      {
        return false;
      }
      cout << "\t\t\tClient: AuthStep 2, dissected server response" << endl;

      if (cipherChallenge.getSize() != (ChallengeSize + crypto_box_MACBYTES)) return false;
      cout << "\t\t\tClient: Signed and encrypted challenge has correct length" << endl;

      if (challengeFromPeer.getSize() != ChallengeSize) return false;
      cout << "\t\t\tClient: Challenge from server has correct length" << endl;
      cout << "\t\t\tClient: Challenge from server is " << toBase64(challengeFromPeer) << endl;

      // check the encrypted challenge
      // THIS PROVES THAT THE SERVER ACTUALLY CONTROLS THE PRIVATE KEY
      sk.setAccess(SodiumSecureMemAccess::RO);
      auto returnedChallenge = sodium->crypto_box_open_easy(cipherChallenge, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      if (!(returnedChallenge.isValid())) return false;
      cout << "\t\t\tClient: returned challenge from server has valid signature" << endl;

      if (!(sodium->memcmp(challengeForPeer, returnedChallenge))) return false;
      cout << "\t\t\tClient: returned challenge from server has correct content" << endl;

      // call the hook for authorizing the server's public key
      if (!(isPeerAcceptable(peerPubKey))) return false;
      cout << "\t\t\tClient: server's public key (==> identity!) accepted!" << endl;

      cout << "\t\t\tClient: AuthStep 2 okay" << endl;
      return true;
    }

    //----------------------------------------------------------------------------

    bool CryptoClient::authStep3()
    {
      // step 3a: send the encrypted and signed challenge from the server
      // back to the server; append an initial nonce for later symmetric
      // encryption and public DH parameter (signed by the client)
      sodium->increment(asymNonce);
      sk.setAccess(SodiumSecureMemAccess::RO);
      ManagedBuffer cipherChallenge = sodium->crypto_box_easy(challengeFromPeer, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);

      sodium->randombytes_buf(symNonce);

      MessageBuilder b;
      b.addManagedMemory(cipherChallenge);
      b.addManagedMemory(symNonce);

      sodium->increment(asymNonce);
      sk.setAccess(SodiumSecureMemAccess::RO);
      auto signedAndEncryptedDH = sodium->crypto_box_easy(dhEx.getMyPublicKey(), asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
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
      sk.setAccess(SodiumSecureMemAccess::RO);
      string _pubDh = sodium->crypto_box_open_easy(data, asymNonce, peerPubKey, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
      if (_pubDh.empty()) return false;
      cout << "\t\t\tClient: server's signed DH key has valid signature" << endl;

      SodiumLib::DH_PublicKey pubDH;
      if (!(pubDH.fillFromString(_pubDh))) return false;
      sessionKey = dhEx.getSharedSecret(pubDH);
      //cout << "\t\t\tClient: session key is " << toBase64(sessionKey) << endl;
      sessionKey.setAccess(SodiumSecureMemAccess::NoAccess);

      cout << "\t\t\tClient: AuthStep 3 okay" << endl;
      return true;

    }

    //----------------------------------------------------------------------------

    bool CryptoClient::cmpServerKeys(const PubKey& srvPubKey) const
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

    //----------------------------------------------------------------------------

  }
}
