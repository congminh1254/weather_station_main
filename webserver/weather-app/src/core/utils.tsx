import { FirebaseApp, FirebaseOptions, initializeApp } from "@firebase/app";


export function getFirebaseApp(): FirebaseApp{
    try {
        const firebaseConfig = require('../firebase-config.json');
        return initializeApp(firebaseConfig as FirebaseOptions);
    }
    catch (e) {
        console.log('No firebase config found');
    }
    return initializeApp();
}