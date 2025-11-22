import './App.css';
import React, { useRef, useState } from 'react';

const API_URL = process.env.REACT_APP_VIZ_URL || 'http://localhost:8000/visualize';
const CONTROL_URL = process.env.REACT_APP_CONTROL_URL || 'http://localhost:8000/control/shutdown';
const UI_WORKER_ID = parseInt(process.env.REACT_APP_UI_WORKER_ID ?? '0', 10);

const defaultParams = {
  layerHeight: 1,
  infillSpacing: 1.0,
  layer: 0,
  showMesh: true,
  showContours: true,
  showInfill: true,
  showRawIntersections: false,
  colorIntersections: false,
};

function App() {
  const cadFileRef = useRef(null);
  const [cadFile, setCadFile] = useState(null);
  const [params, setParams] = useState(defaultParams);
  const [preview, setPreview] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [info, setInfo] = useState('');
  const [meta, setMeta] = useState(null);
  const [shuttingDown, setShuttingDown] = useState(false);

  const selectCAD = () => {
    cadFileRef.current?.click();
  };

  const handleFileChange = (event) => {
    const file = event.target.files?.[0];
    if (file) {
      setCadFile(file);
      setError('');
    }
  };

  const handleParamChange = (key) => (event) => {
    const value = event.target.type === 'checkbox' ? event.target.checked : event.target.value;
    setParams((prev) => ({ ...prev, [key]: value }));
  };

  const visualize = async () => {
    if (!cadFile) {
      setError('Please import an STL to visualize.');
      return;
    }

    setLoading(true);
    setError('');
    setInfo('');
    setPreview(null);

    const formData = new FormData();
    formData.append('stl', cadFile);
    Object.entries(params).forEach(([key, value]) => formData.append(key, value));

    try {
      const response = await fetch(API_URL, { method: 'POST', body: formData });
      if (!response.ok) {
        const payload = await response.json().catch(() => ({}));
        throw new Error(payload.error || 'Failed to render preview.');
      }
      const payload = await response.json();
      setPreview(payload.image);
      setMeta(payload.meta || null);
    } catch (err) {
      setError(err.message || 'Unable to reach visualization service.');
    } finally {
      setLoading(false);
    }
  };

  const requestShutdown = async () => {
    setShuttingDown(true);
    setError('');
    setInfo('');
    try {
      const response = await fetch(CONTROL_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ workerId: UI_WORKER_ID }),
      });
      if (!response.ok) {
        const payload = await response.json().catch(() => ({}));
        throw new Error(payload.error || 'Failed to signal shutdown.');
      }
      const payload = await response.json();
      setPreview(null);
      setMeta(null);
      setInfo(payload.message || 'Shutdown signal sent to controller.');
    } catch (err) {
      setError(err.message || 'Unable to reach controller dispatcher.');
    } finally {
      setShuttingDown(false);
    }
  };

  return (
    <div className="app-shell">
      <div className="glass-card">
        <header className="header">
          <div>
            <p className="eyebrow">Path Planner</p>
            <h1>CAD to Layer Preview</h1>
            <p className="subhead">Upload an STL, tweak slice settings, and view the Matplotlib output without leaving the app.</p>
          </div>
          <div className="import-group">
            <button className="button ghost" onClick={selectCAD}>
              {cadFile ? 'Replace CAD' : 'Import CAD'}
            </button>
            <p className="filename">{cadFile ? cadFile.name : 'No file selected'}</p>
            <input type="file" accept=".stl" ref={cadFileRef} onChange={handleFileChange} style={{ display: 'none' }} />
          </div>
        </header>

        <div className="grid">
          <section className="panel controls">
            <div className="panel-title">Slice controls</div>
            <div className="fields">
              <label className="field">
                <span>Layer height (mm)</span>
                <input type="number" min="0" step="0.1" value={params.layerHeight} onChange={handleParamChange('layerHeight')} />
              </label>
              <label className="field">
                <span>Infill spacing (mm)</span>
                <input type="number" min="0" step="0.1" value={params.infillSpacing} onChange={handleParamChange('infillSpacing')} />
              </label>
              <label className="field">
                <span>Layer index</span>
                <input type="number" min="0" step="1" value={params.layer} onChange={handleParamChange('layer')} />
              </label>
            </div>

            <div className="toggles">
              <label className="toggle">
                <input type="checkbox" checked={params.showMesh} onChange={handleParamChange('showMesh')} />
                <span>Show mesh</span>
              </label>
              <label className="toggle">
                <input type="checkbox" checked={params.showContours} onChange={handleParamChange('showContours')} />
                <span>Show contours</span>
              </label>
              <label className="toggle">
                <input type="checkbox" checked={params.showInfill} onChange={handleParamChange('showInfill')} />
                <span>Show infill</span>
              </label>
              <label className="toggle">
                <input type="checkbox" checked={params.showRawIntersections} onChange={handleParamChange('showRawIntersections')} />
                <span>Raw intersections</span>
              </label>
              <label className="toggle">
                <input type="checkbox" checked={params.colorIntersections} onChange={handleParamChange('colorIntersections')} />
                <span>Color intersecting tris</span>
              </label>
            </div>

            <div className="actions">
              <button className="button primary" onClick={visualize} disabled={loading}>
                {loading ? 'Rendering...' : 'Render preview'}
              </button>
              <p className="hint">Runs the Python Matplotlib script via the visualization API.</p>
              <button className="button danger" onClick={requestShutdown} disabled={shuttingDown}>
                {shuttingDown ? 'Shutting down...' : 'Shutdown controller'}
              </button>
            </div>
            {error && <p className="error">{error}</p>}
            {info && !error && <p className="info">{info}</p>}
          </section>

          <section className="panel preview">
            <div className="panel-title">Preview</div>
            <div className="preview-frame">
              {loading && <div className="placeholder">Crunching paths...</div>}
              {!loading && preview && <img src={preview} alt="Matplotlib layer preview" />}
              {!loading && !preview && <div className="placeholder">Import a CAD file and render to see the plot.</div>}
            </div>
            {meta && (
              <div className="meta">
                <span>Layers: {meta.layers}</span>
                <span>Selected: {meta.selectedLayer}</span>
                <span>z: {meta.zHeight}mm</span>
              </div>
            )}
          </section>
        </div>
      </div>
    </div>
  );
}

export default App;
