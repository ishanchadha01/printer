import { render, screen } from '@testing-library/react';
import App from './App';

test('renders upload controls', () => {
  render(<App />);
  expect(screen.getByText(/CAD to Layer Preview/i)).toBeInTheDocument();
  expect(screen.getByText(/Import CAD/i)).toBeInTheDocument();
  expect(screen.getByText(/Render preview/i)).toBeInTheDocument();
});
