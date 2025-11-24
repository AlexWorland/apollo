<script setup>
import {ref, inject} from 'vue'

const $t = inject('i18n').t;

const props = defineProps([
  'platform',
  'config',
])

// Default values (mirrors config.cpp/config.h defaults)
const defaults = {
  auto_bitrate_min_kbps: 500,
  auto_bitrate_max_kbps: 0,
  auto_bitrate_adjustment_interval_ms: 3000,
  auto_bitrate_loss_severe_pct: 10,
  auto_bitrate_loss_moderate_pct: 5,
  auto_bitrate_loss_mild_pct: 1,
  auto_bitrate_decrease_severe_pct: 25,
  auto_bitrate_decrease_moderate_pct: 12,
  auto_bitrate_decrease_mild_pct: 5,
  auto_bitrate_increase_good_pct: 5,
  auto_bitrate_good_stability_ms: 5000,
  auto_bitrate_increase_min_interval_ms: 3000,
  auto_bitrate_poor_status_cap_pct: 25,
}

const config = ref(props.config)

// Seed missing fields with defaults so "reset" applies immediately at runtime
Object.keys(defaults).forEach(key => {
  if (config.value[key] === undefined) {
    config.value[key] = defaults[key];
  }
});

const resetToDefaults = () => {
  Object.keys(defaults).forEach(key => {
    config.value[key] = defaults[key];
  });
}

const validateMinBitrate = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 500) {
    event.target.setCustomValidity($t('config.auto_bitrate_min_kbps_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateMaxBitrate = (event) => {
  const value = parseInt(event.target.value);
  const minValue = parseInt(config.value.auto_bitrate_min_kbps) || 500;
  if (value !== 0 && (isNaN(value) || value < minValue)) {
    event.target.setCustomValidity($t('config.auto_bitrate_max_kbps_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateAdjustmentInterval = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 1000) {
    event.target.setCustomValidity($t('config.auto_bitrate_adjustment_interval_ms_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateGoodStability = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 0) {
    event.target.setCustomValidity($t('config.auto_bitrate_good_stability_ms_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validateIncreaseMinInterval = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 1000) {
    event.target.setCustomValidity($t('config.auto_bitrate_increase_min_interval_ms_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}

const validatePercentage = (event) => {
  const value = parseInt(event.target.value);
  if (isNaN(value) || value < 0 || value > 100) {
    event.target.setCustomValidity($t('config.auto_bitrate_percentage_error'));
  } else {
    event.target.setCustomValidity('');
  }
  event.target.reportValidity();
}
</script>

<template>
  <div id="auto-bitrate" class="config-page">
    <h5 class="mb-3">{{ $t('config.auto_bitrate_section_bounds') }}</h5>
    
    <!-- Minimum Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_min_kbps" class="form-label">{{ $t('config.auto_bitrate_min_kbps') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_min_kbps"
        v-model.number="config.auto_bitrate_min_kbps"
        min="500"
        step="100"
        @input="validateMinBitrate"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_min_kbps_desc') }}</div>
    </div>

    <!-- Maximum Bitrate -->
    <div class="mb-3">
      <label for="auto_bitrate_max_kbps" class="form-label">{{ $t('config.auto_bitrate_max_kbps') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_max_kbps"
        v-model.number="config.auto_bitrate_max_kbps"
        min="0"
        step="1000"
        @input="validateMaxBitrate"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_max_kbps_desc') }}</div>
    </div>

    <hr class="my-4">

    <h5 class="mb-3">{{ $t('config.auto_bitrate_section_timing') }}</h5>

    <!-- Adjustment Interval -->
    <div class="mb-3">
      <label for="auto_bitrate_adjustment_interval_ms" class="form-label">{{ $t('config.auto_bitrate_adjustment_interval_ms') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_adjustment_interval_ms"
        v-model.number="config.auto_bitrate_adjustment_interval_ms"
        min="1000"
        step="500"
        @input="validateAdjustmentInterval"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_adjustment_interval_ms_desc') }}</div>
    </div>

    <!-- Good Stability Window -->
    <div class="mb-3">
      <label for="auto_bitrate_good_stability_ms" class="form-label">{{ $t('config.auto_bitrate_good_stability_ms') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_good_stability_ms"
        v-model.number="config.auto_bitrate_good_stability_ms"
        min="0"
        step="500"
        @input="validateGoodStability"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_good_stability_ms_desc') }}</div>
    </div>

    <!-- Increase Minimum Interval -->
    <div class="mb-3">
      <label for="auto_bitrate_increase_min_interval_ms" class="form-label">{{ $t('config.auto_bitrate_increase_min_interval_ms') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_increase_min_interval_ms"
        v-model.number="config.auto_bitrate_increase_min_interval_ms"
        min="1000"
        step="500"
        @input="validateIncreaseMinInterval"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_increase_min_interval_ms_desc') }}</div>
    </div>

    <hr class="my-4">

    <h5 class="mb-3">{{ $t('config.auto_bitrate_section_adjustments') }}</h5>

    <!-- Decrease Severe Percentage -->
    <div class="mb-3">
      <label for="auto_bitrate_decrease_severe_pct" class="form-label">{{ $t('config.auto_bitrate_decrease_severe_pct') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_decrease_severe_pct"
        v-model.number="config.auto_bitrate_decrease_severe_pct"
        min="0"
        max="100"
        step="1"
        @input="validatePercentage"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_decrease_severe_pct_desc') }}</div>
    </div>

    <!-- Decrease Moderate Percentage -->
    <div class="mb-3">
      <label for="auto_bitrate_decrease_moderate_pct" class="form-label">{{ $t('config.auto_bitrate_decrease_moderate_pct') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_decrease_moderate_pct"
        v-model.number="config.auto_bitrate_decrease_moderate_pct"
        min="0"
        max="100"
        step="1"
        @input="validatePercentage"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_decrease_moderate_pct_desc') }}</div>
    </div>

    <!-- Decrease Mild Percentage -->
    <div class="mb-3">
      <label for="auto_bitrate_decrease_mild_pct" class="form-label">{{ $t('config.auto_bitrate_decrease_mild_pct') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_decrease_mild_pct"
        v-model.number="config.auto_bitrate_decrease_mild_pct"
        min="0"
        max="100"
        step="1"
        @input="validatePercentage"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_decrease_mild_pct_desc') }}</div>
    </div>

    <!-- Increase Good Percentage -->
    <div class="mb-3">
      <label for="auto_bitrate_increase_good_pct" class="form-label">{{ $t('config.auto_bitrate_increase_good_pct') }}</label>
      <input
        type="number"
        class="form-control"
        id="auto_bitrate_increase_good_pct"
        v-model.number="config.auto_bitrate_increase_good_pct"
        min="0"
        max="100"
        step="1"
        @input="validatePercentage"
      />
      <div class="form-text">{{ $t('config.auto_bitrate_increase_good_pct_desc') }}</div>
    </div>

    <hr class="my-4">

    <!-- Reset to Defaults Button -->
    <div class="mb-3">
      <button type="button" class="btn btn-secondary" @click="resetToDefaults">
        {{ $t('config.auto_bitrate_reset_to_defaults') }}
      </button>
      <div class="form-text mt-2">{{ $t('config.auto_bitrate_reset_to_defaults_desc') }}</div>
    </div>

    <div class="alert alert-info">
      <i class="fa-solid fa-xl fa-circle-info"></i>
      {{ $t('config.auto_bitrate_info') }}
    </div>
  </div>
</template>

<style scoped>
</style>
