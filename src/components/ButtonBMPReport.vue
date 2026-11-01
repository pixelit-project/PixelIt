<template>
    <v-dialog v-model="dialog" :persistent="submitting" max-width="480">
        <template v-slot:activator="{ on, attrs }">
            <v-btn small outlined color="primary" class="mt-3 mb-2" v-bind="attrs" v-on="on" :disabled="!validIconId || $demoMode" title="Report icon" aria-label="Report icon">
                <v-icon left small>mdi-flag-outline</v-icon>
                Report
            </v-btn>
        </template>
        <v-card>
            <v-card-title>Report icon</v-card-title>
            <v-card-text>
                <p class="font-weight-medium">{{ bmp.name }} · #{{ bmp.id }}</p>
                <p>A moderator will review your report. Reporting does not delete the icon.</p>
                <v-select
                    attach
                    v-model="reasonType"
                    :items="reportReasons"
                    label="Reason"
                    outlined
                    :disabled="submitting"
                />
                <v-textarea
                    ref="reasonInput"
                    v-model="reason"
                    :label="reasonType === 'other' ? 'Additional details (required)' : 'Additional details (optional)'"
                    :placeholder="reasonType === 'duplicate' ? 'If known, include the ID of the other icon.' : 'Add details to help the moderator.'"
                    outlined
                    rows="4"
                    :counter="maximumReasonLength"
                    :maxlength="maximumReasonLength"
                    :rules="reasonRules"
                    :disabled="submitting"
                />
                <v-alert v-if="errorMessage" type="error" dense outlined role="alert">{{ errorMessage }}</v-alert>
            </v-card-text>
            <v-card-actions>
                <v-spacer />
                <v-btn text :disabled="submitting" @click="closeDialog">Cancel</v-btn>
                <v-btn color="primary" text :loading="submitting" :disabled="!canSubmit" @click="submitReport">Send report</v-btn>
            </v-card-actions>
        </v-card>
    </v-dialog>
</template>

<script>
import { notify } from '../utils/feedback';

const MAXIMUM_REASON_LENGTH = 1000;
const REPORT_REASONS = [
    { text: 'Duplicate icon', value: 'duplicate' },
    { text: 'Incorrect display', value: 'incorrect' },
    { text: 'Inappropriate content', value: 'inappropriate' },
    { text: 'Remove my own upload', value: 'own-upload' },
    { text: 'Other reason', value: 'other' },
];

export default {
    props: {
        bmp: { type: Object, required: true },
    },
    data() {
        return {
            dialog: false,
            reason: '',
            reasonType: null,
            reportReasons: REPORT_REASONS,
            errorMessage: '',
            submitting: false,
            requestController: null,
        };
    },
    computed: {
        validIconId() {
            const iconId = Number(this.bmp.id);
            return Number.isSafeInteger(iconId) && iconId > 0;
        },
        selectedReason() {
            return REPORT_REASONS.find(reason => reason.value === this.reasonType);
        },
        maximumReasonLength() {
            return MAXIMUM_REASON_LENGTH - (this.selectedReason ? this.selectedReason.text.length + 2 : 0);
        },
        formattedReason() {
            if (!this.selectedReason) {
                return '';
            }
            const details = typeof this.reason === 'string' ? this.reason.trim() : '';
            return details ? `${this.selectedReason.text}: ${details}` : this.selectedReason.text;
        },
        validReason() {
            const hasDetails = typeof this.reason === 'string' && this.reason.trim().length > 0;
            const detailsLength = typeof this.reason === 'string' ? this.reason.length : 0;
            return Boolean(this.selectedReason) && (this.reasonType !== 'other' || hasDetails) &&
                detailsLength <= this.maximumReasonLength && this.formattedReason.length <= MAXIMUM_REASON_LENGTH;
        },
        canSubmit() {
            return this.validIconId && this.validReason && !this.submitting && !this.$demoMode;
        },
        reasonRules() {
            return [
                value => this.reasonType !== 'other' || (typeof value === 'string' && value.trim().length > 0) || 'Please provide additional details.',
                value => !value || value.length <= this.maximumReasonLength || `Use at most ${this.maximumReasonLength} characters.`,
            ];
        },
    },
    watch: {
        dialog(value) {
            if (value) {
                this.reason = '';
                this.reasonType = null;
                this.errorMessage = '';
                this.$nextTick(() => {
                    if (this.$refs.reasonInput) {
                        this.$refs.reasonInput.resetValidation();
                    }
                });
            }
            this.$emit('dialog-change', value);
        },
    },
    beforeDestroy() {
        if (this.requestController) {
            this.requestController.abort();
        }
        this.$emit('dialog-change', false);
    },
    methods: {
        closeDialog() {
            if (!this.submitting) {
                this.dialog = false;
            }
        },
        async submitReport() {
            if (!this.canSubmit) {
                return;
            }
            this.submitting = true;
            this.errorMessage = '';
            const controller = new AbortController();
            this.requestController = controller;
            try {
                const baseURL = this.$apiServerBaseURL.replace(/\/$/, '');
                const response = await fetch(`${baseURL}/reports`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json', Accept: 'application/json', Client: this.$client },
                    body: JSON.stringify({ iconId: Number(this.bmp.id), reason: this.formattedReason }),
                    signal: controller.signal,
                });
                if (response.status !== 202) {
                    const messages = {
                        400: 'The report is invalid. Please check your reason.',
                        404: 'The icon was not found or reporting is unavailable. Refresh the gallery and try again.',
                        429: 'Too many reports. Please try again later.',
                        503: 'Reporting is temporarily unavailable. Please try again later.',
                    };
                    throw new Error(messages[response.status] || 'The report could not be sent. Please try again.');
                }
                if (!this._isBeingDestroyed && !this._isDestroyed) {
                    this.dialog = false;
                    this.reason = '';
                    notify('Report received. A moderator will review the icon.', 'success');
                }
            } catch (error) {
                if (!controller.signal.aborted && !this._isBeingDestroyed && !this._isDestroyed) {
                    this.errorMessage = error.message || 'The report could not be sent. Please try again.';
                }
            } finally {
                this.submitting = false;
                this.requestController = null;
            }
        },
    },
};
</script>
