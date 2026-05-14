import ConfigRow from '../components/ConfigRow.jsx';
import {
  EXPECTATION_TYPES,
  EXPECTATION_PARAMS,
  defaultExpectationParams,
} from '../constants/expectations.js';

export default function ExpectationRow({ value, onChange, onRemove }) {
  return (
    <ConfigRow
      types={EXPECTATION_TYPES}
      getParamsForType={(t) => EXPECTATION_PARAMS[t]}
      getDefaultParams={defaultExpectationParams}
      value={value}
      onChange={onChange}
      onRemove={onRemove}
    />
  );
}
