import ConfigRow from '../components/ConfigRow.jsx';
import {
  PERTURBATION_TYPES,
  PERTURBATION_PARAMS,
  defaultParamsFor,
} from '../constants/perturbations.js';

export default function PerturbationRow({ value, onChange, onRemove }) {
  return (
    <ConfigRow
      types={PERTURBATION_TYPES}
      getParamsForType={(t) => PERTURBATION_PARAMS[t]}
      getDefaultParams={defaultParamsFor}
      value={value}
      onChange={onChange}
      onRemove={onRemove}
    />
  );
}
