import './App.css';

import React, { useRef, useState } from 'react'

function App() {
  const cadFileRef = useRef(null);

  const selectCAD = () => {
    cadFileRef.current.click();
  }

  const handleFileChange = (event) => {
    const file = event.target.files[0];
    if (file) {
      console.log('Selected file:', file);
      // You can process the file here (e.g., read it using FileReader)
    }
  }

  return (
    <div className="App">
      <button onClick={selectCAD}>
        Import CAD
      </button>
      <input
        type="file"
        accept=".stl"
        ref={cadFileRef}
        onChange={handleFileChange}
        style={{ display: 'none' }}
      />
    </div>
  );
}

export default App;
